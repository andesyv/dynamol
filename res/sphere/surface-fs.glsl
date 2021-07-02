#version 450
#extension GL_ARB_shading_language_include : require
#include "/defines"
#include "/globals.glsl"

#define BIAS 0.001
#define END_PLANE 65535.0

layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 inverseModelViewProjectionMatrix;
uniform mat3 normalMatrix;
uniform float sharpness;
uniform uint coloring;
uniform bool environment;
uniform bool lens;

uniform vec3 lightPosition;
uniform vec3 diffuseMaterial;
uniform vec3 ambientMaterial;
uniform vec3 specularMaterial;
uniform float shininess;
uniform vec2 focusPosition;

uniform float interpolation = 0.0;

layout(binding = 0) uniform usampler2D offsetTexture;
layout(binding = 1) uniform usampler2D offsetTexture2;
layout(binding = 2) uniform sampler2D positionTexture;
layout(binding = 3) uniform sampler2D normalTexture;
layout(binding = 4) uniform sampler2D positionTexture2;
layout(binding = 5) uniform sampler2D normalTexture2;

uniform uint gridScale = 1;
uniform uint gridDepth = 1;
uniform vec3 minb;
uniform vec3 maxb;
uniform float time = 0.0;
uniform float var1 = 0.0;
uniform float var2 = 0.0;
uniform float var3 = 0.0;

in vec4 gFragmentPosition;
#ifdef VISUALIZE_OVERLAPS
layout(location = 0) out vec4 fragColor;
#endif
out vec4 surfacePosition;
out vec4 surfaceNormal;
out vec4 surfaceDiffuse;
// out vec4 sphereDiffuse;
// out vec4 fragColor;

struct Element
{
	vec3 color;
	float radius;
};

struct Residue
{
	vec4 color;
};

struct Chain
{
	vec4 color;
};

layout(std140, binding = 0) uniform elementBlock
{
	Element elements[32];
};

layout(std140, binding = 1) uniform residueBlock
{
	Residue residues[32];
};

layout(std140, binding = 2) uniform chainBlock
{
	Chain chains[64];
};

struct BufferEntry
{
	float near;
	float far;
	vec3 center;
	uint id;
	uint previous;
	float radius;
	float sharpness;
	float weight;
};

struct Cell
{
	// Sum of positions in this cell
	uvec4 pos;
	// Count of particles in this cell
	uint count;
	// Offset to first child cell.
	// Child cells: [offset, offset + 1, offset + 2, ..., offset + 7]
	// uint childOffset;
};

layout(std430, binding = 7) buffer scenegraphBuffer
{
	Cell cells[];
};

layout(binding = 1) uniform atomic_uint count;

layout(std430, binding = 1) buffer intersectionBuffer
{
	BufferEntry intersections[];
};

layout(binding = 2) uniform atomic_uint count2;

layout(std430, binding = 2) buffer intersectionBuffer2
{
	BufferEntry intersections2[];
};

// layout(std430, binding = 2) buffer statisticsBuffer
// {
// 	uint intersectionCount;
// 	uint totalPixelCount;
// 	uint totalEntryCount;
// 	uint maximumEntryCount;
// };

struct Sphere
{			
	bool hit;
	vec3 near;
	vec3 far;
	vec3 normal;
};

float calcDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}

void swap(inout uint a, inout uint b) {
	uint temp = a;
	a = b;
	b = temp;
}

void main()
{
	uint offset = texelFetch(offsetTexture,ivec2(gl_FragCoord.xy),0).r;
	uint offset2 = texelFetch(offsetTexture2,ivec2(gl_FragCoord.xy),0).r;
	// Note: Offset == 0 doesn't mean offset2 != 0
	if (offset == 0 && offset2 == 0)
		discard;

	vec4 position = texelFetch(positionTexture,ivec2(gl_FragCoord.xy),0);
	vec4 normal = texelFetch(normalTexture,ivec2(gl_FragCoord.xy),0);
	vec4 position2 = texelFetch(positionTexture2,ivec2(gl_FragCoord.xy),0);
	vec4 normal2 = texelFetch(normalTexture2,ivec2(gl_FragCoord.xy),0);

	vec4 fragCoord = gFragmentPosition;
	fragCoord /= fragCoord.w;
	
	vec4 near = inverseModelViewProjectionMatrix*vec4(fragCoord.xy,-1.0,1.0);
	near /= near.w;

	vec4 far = inverseModelViewProjectionMatrix*vec4(fragCoord.xy,1.0,1.0);
	far /= far.w;

	vec3 V = normalize(far.xyz-near.xyz);

	const uint maxEntries = 128;
	uint entryCount = 0;
	uint entryCount2 = 0;
	uint indices[maxEntries];
	uint indices2[maxEntries];

	// Traverse a-buffer and extract entries. (0 means empty)
	while (offset > 0)
	{
		indices[entryCount++] = offset;
		offset = intersections[offset].previous;
	}
	while (offset2 > 0)
	{
		indices2[entryCount2++] = offset2;
		offset2 = intersections2[offset2].previous;
	}

	// Simplified algorithm if we only have a single list
	const bool bFirstEmpty = entryCount == 0;
	const bool bSecondEmpty = entryCount2 == 0;

// #ifdef VISUALIZE_OVERLAPS
// 	fragColor = vec4(vec3(entryCount2) / 128.0, 1.0);
// 	return;
// #endif



	/** Find closest position and sphere march end step.
	 * Sphere march stops marching when past the closest position
	 * Initial closest position is set to closest inner sphere
	 * 
	 * Note: position can still be END_PLANE when entryCount != 0, because
	 * position is inner radius and entryCount is outer radius.
	 * Likewise: position == END_PLANE when entryCount == 0
	 */
	vec4 closestPosition;
	vec3 closestNormal;
	// If second LOD is empty, use first
	if (entryCount2 == 0 || position2.w >= END_PLANE) {
		closestPosition = position;
		closestNormal = normal.xyz;
	// If first LOD is empty, use second
	} else if (entryCount == 0 || position.w >= END_PLANE) {
		closestPosition = position2;
		closestNormal = normal2.xyz;
	// If none are empty, choose closest:
	} else {
		// if (position.w > position2.w) {
		// 	closestPosition = position;
		// 	closestNormal = normal.xyz;
		// } else {
		// 	closestPosition = position2;
		// 	closestNormal = normal2.xyz;
		// }
		closestPosition = mix(position, position2, interpolation);
		closestNormal = mix(normal, normal2, interpolation).xyz;
	}

	float sharpnessFactor = 1.0;
	vec3 ambientColor = ambientMaterial;
	vec3 diffuseColor = vec3(1.0,1.0,1.0);
	vec3 specularColor = specularMaterial;
	vec3 diffuseSphereColor = vec3(1.0,1.0,1.0);

	// Start index is the index of the sphere before the intersecting spheres
	uint startIndex = 0;
	uint startIndex2 = 0;

	// End index is the index of the sphere after the intersecting spheres
	uint endIndex = 0;
	uint endIndex2 = 0;

	const uint maxEntryCount = max(entryCount, entryCount2);

	// sphere tracing parameters
	const uint maximumSteps = 32; // maximum number of steps
	const float eps = 0.0125; // threshold for detected intersection
	const float omega = 1.2; // over-relaxation factor
	const uint maximumIntersections = 128;

	const float s = sharpness*sharpnessFactor;

// #ifdef VISUALIZE_OVERLAPS
// 	fragColor = vec4(vec3(entryCount2) / 100.0, 1.0);
// 	return;
// #endif

	/** N^2 - selection sort
	 * N => 2 * n/2 => 2 * (n/2)^2 = n^2 / 2
	 * ^ Splitting the sorting into two lists, halves the complexity
	 */

	// Loop for +1 past the end such that endIndex can extend to 1 past the end of the list.
	// (Since our intersecting list is in range [start, end-1])
	for (uint currentIndex = 0; currentIndex <= maxEntryCount; ++currentIndex) {
		if (!bFirstEmpty && currentIndex <= entryCount) {
			// Incremental sort (first list)
			/// Don't need to sort on the last index because it will be sorted by that point
			if (currentIndex < entryCount - 1) {
				uint minimumIndex = currentIndex;

				// Find minimum index (based on near distance)
				for(uint i = minimumIndex+1; i < entryCount; i++)
					if(intersections[indices[i]].near < intersections[indices[minimumIndex]].near)
						minimumIndex = i;
				
				// Selection sort swap:
				if (minimumIndex != currentIndex)
					swap(indices[minimumIndex], indices[currentIndex]);
			}

			// Increment endIndex (first list)
			// && endIndex+1 - startIndex < maximumIntersections
			while (endIndex < currentIndex && intersections[indices[endIndex]].near <= intersections[indices[startIndex]].far)
				++endIndex;
		}

		if (!bSecondEmpty && currentIndex <= entryCount2) {
			// Incremental sort (second list)
			if (currentIndex < entryCount2 - 1) {
				uint minimumIndex = currentIndex;

				// Find minimum index (based on near distance)
				for(uint i = minimumIndex+1; i < entryCount2; i++)
					if(intersections2[indices2[i]].near < intersections2[indices2[minimumIndex]].near)
						minimumIndex = i;
				
				// Selection sort swap:
				if (minimumIndex != currentIndex)
					swap(indices2[minimumIndex], indices2[currentIndex]);
			}

			// Increment endIndex (second list)
			while (endIndex2 < currentIndex && intersections2[indices2[endIndex2]].near <= intersections2[indices2[startIndex2]].far)
				++endIndex2;
		}

		// If we've yet to find a set of intersecting spheres, keep looping
		if (
			(!bFirstEmpty && endIndex < entryCount && intersections[indices[endIndex]].near <= intersections[indices[startIndex]].far) ||
			(!bSecondEmpty && endIndex2 < entryCount2 && intersections2[indices2[endIndex2]].near <= intersections2[indices2[startIndex2]].far)
		)
			continue;

		// Note: At this point both ranges of sphere lists should either have reached an end of the span of overlap,
		// or have reached the end. In both cases it should be safe to continue with sphere tracing.

		// Q: Why +1?
		// A: We (for some reason) check the inner range of the intersecting spheres [start+1, end-1]
		uint ii = indices[startIndex];
		uint ii2 = indices2[startIndex2];
		// Note: Near as min when only one list is in effect gives wrong result because one of them is 0
		float nearDistance = bSecondEmpty ? intersections[ii].near : bFirstEmpty ? intersections2[ii2].near
							: min(intersections[ii].near, intersections2[ii2].near);
		float farDistance = bSecondEmpty ? intersections[indices[endIndex-1]].far : bFirstEmpty ? intersections2[indices2[endIndex2-1]].far
							: max(intersections[indices[endIndex-1]].far, intersections2[indices2[endIndex2-1]].far);
		// float nearDistance = intersections2[ii2].near;
		// float farDistance = intersections2[indices2[endIndex2-1]].far;

		float maximumDistance = (farDistance-nearDistance);
		float surfaceDistance = 1.0;

		vec4 rayOrigin = vec4(near.xyz,0.);
		vec4 rayDirection = vec4(V,1.0);
		vec4 currentPosition;
		
		vec4 candidatePosition = rayOrigin + rayDirection * nearDistance;
		vec3 candidateNormal = vec3(0.0);
		vec3 candidateColor = vec3(0.0);
		float candidateValue = 0.0;

		float minimumDistance = maximumDistance;

		uint currentStep = 0;			
		float t = nearDistance;

		while (++currentStep <= maximumSteps && t <= farDistance)
		{    
			currentPosition = rayOrigin + rayDirection*t;

			if (currentPosition.w > closestPosition.w)
				break;

			float sumValue = 0.0;
			vec3 sumNormal = vec3(0.0);
			vec3 sumColor = vec3(0.0);
			
			// sum contributions of atoms in the neighborhood (for first surface)
			if (!bFirstEmpty) {
				for (uint j = startIndex; j < endIndex; j++)
				{
					uint ij = indices[j];
					uint id = intersections[ij].id;
					uint elementId = bitfieldExtract(id,0,8);

					vec3 aj = intersections[ij].center;
					float rj = intersections[ij].radius; // elements[elementId].radius;
					float weight = intersections[ij].weight;
					// float sphereSharpness = intersections[ij].sharpness;

					vec3 atomOffset = currentPosition.xyz-aj;
					float atomDistanceSquared = dot(atomOffset, atomOffset)/(rj*rj);

					float atomValue = exp(-s*atomDistanceSquared) * weight;
					vec3 atomNormal = atomValue*normalize(atomOffset);
					
					sumValue += atomValue;
					sumNormal += atomNormal;
				}
			}
			
			// sum contributions of atoms in the neighborhood (for second surface)
			if (!bSecondEmpty) {
				for (uint j = startIndex2; j < endIndex2; j++)
				{
					uint ij = indices2[j];
					uint id = intersections2[ij].id;
					uint elementId = bitfieldExtract(id,0,8);

					vec3 aj = intersections2[ij].center;
					float rj = intersections2[ij].radius; // elements[elementId].radius;
					float weight = intersections2[ij].weight;
					// float sphereSharpness = intersections[ij].sharpness;

					vec3 atomOffset = currentPosition.xyz-aj;
					float atomDistanceSquared = dot(atomOffset, atomOffset)/(rj*rj);

					float atomValue = exp(-s*atomDistanceSquared) * weight;
					vec3 atomNormal = atomValue*normalize(atomOffset);
					
					sumValue += atomValue;
					sumNormal += atomNormal;
				}
			}
			
			/** -1 because were searching for the distance to the surface when p(x) = 1.0
				* Distance is defined as d(x) = p(x) - t, and as described in paper we estimate
				* t = 1
				*/
			surfaceDistance = sqrt(-log(sumValue) / (s))-1.0;
			// surfaceDistance = lerp(sqrt(-log(sumValue) / (s))-1.0, sqrt(-log(sumValue2) / (s))-1.0, interpolation)

			if (surfaceDistance < eps)
			{
				// Note: Unessesary check because `if (currentPosition.w > closestPosition.w) break;` prevents this from happening
				// if (currentPosition.w <= closestPosition.w)
				closestPosition = currentPosition;
				closestNormal = sumNormal;
				break;
			}

			// Note: Commenting out this makes a very cool effect. :o
			if (surfaceDistance < minimumDistance)
			{
				minimumDistance = surfaceDistance;
				candidatePosition = currentPosition;
				candidateNormal = sumNormal;
				candidateColor = sumColor;
				candidateValue = sumValue;
			}

			// Over-relaxation according to the approach described by Keinert et al.
			// However, we simply skip overstepping correction, since it is basically invisible.
			// Benjamin Keinert, Henry Sch�fer, Johann Kornd�rfer, Urs Ganse, and Marc Stamminger.
			// Enhanced Sphere Tracing. Proceedings of Smart Tools and Apps for Graphics (Eurographics Italian Chapter Conference), pp. 1--8, 2014. 
			// http://dx.doi.org/10.2312/stag.20141233
			// t += surfaceDistance*omega;
			t += surfaceDistance*omega;
		}
		
		// Only check for new closest position if all iterations passed (if t never overshot farDistance)
		if (currentStep > maximumSteps)
		{
			// Note: Unessesary check because `if (currentPosition.w > closestPosition.w) break;` prevents this from happening
			// if (candidatePosition.w <= closestPosition.w)
			closestPosition = candidatePosition;
			closestNormal = candidateNormal;
		}

		// If lists aren't empty, increment closest
		if (!bFirstEmpty && !bSecondEmpty) {
			if (intersections[ii].near < intersections2[ii2].near)
				++startIndex;
			else
				++startIndex2;
		// Otherwise increment both (only one of them is actually used)
		} else {
			++startIndex;
			++startIndex2;
		}
	}

	if (closestPosition.w >= 65535.0)
		discard;

#ifdef VISUALIZE_OVERLAPS
	fragColor = vec4(vec3(closestPosition.w) / 100.0, 1.0);
	return;
#endif
	

// #ifdef NORMAL		
// 	vec3 N = normalize(closestNormal);
	
// 	// https://medium.com/@bgolus/normal-mapping-for-a-triplanar-shader-10bf39dca05a
// 	vec3 blend = abs( N );
// 	blend = normalize(max(blend, 0.00001)); // Force weights to sum to 1.0
// 	float b = (blend.x + blend.y + blend.z);
// 	blend /= vec3(b, b, b);	
	
// 	vec2 uvX = closestPosition.zy*0.5;
// 	vec2 uvY = closestPosition.xz*0.5;
// 	vec2 uvZ = closestPosition.xy*0.5;

// 	vec3 normalX = 2.0*texture(bumpTexture,uvX).xyz - 1.0;
// 	vec3 normalY = 2.0*texture(bumpTexture,uvY).xyz - 1.0;
// 	vec3 normalZ = 2.0*texture(bumpTexture,uvZ).xyz - 1.0;

// 	normalX = vec3(0.0, normalX.yx);
// 	normalY = vec3(normalY.x, 0.0, normalY.y);
// 	normalZ = vec3(normalZ.xy, 0.0);

// 	vec3 worldNormal = normalize(N + normalX.xyz * blend.x + normalY.xyz * blend.y + normalZ.xyz * blend.z);

// 	closestNormal = worldNormal;
// #endif

	vec4 cp = modelViewMatrix*vec4(closestPosition.xyz, 1.0);
	cp = cp / cp.w;

	surfacePosition = closestPosition;

	closestNormal.xyz = normalMatrix*closestNormal.xyz;
	closestNormal.xyz = normalize(closestNormal.xyz);
	surfaceNormal = vec4(closestNormal.xyz,cp.z);

	// vec3 col = vec3(1.0 - closestPosition.w / 20.0);
	// float phong = max(dot(closestNormal.xyz, vec3(0, .0, 1.)), 0.15);
	// // fragColor = vec4(vec3(entryCount) / 128.0, 1.0);
	// fragColor = vec4(phong * col, 1.0);

// #ifdef MATERIAL
// 	vec3 materialColor = texture( materialTexture , closestNormal.xy*0.5+0.5 ).rgb;
// 	diffuseColor *= materialColor;
// #endif

	surfaceDiffuse = vec4(diffuseColor,1.0);
	// sphereDiffuse = vec4(diffuseSphereColor,1.0);
	gl_FragDepth = calcDepth(closestPosition.xyz);
}
