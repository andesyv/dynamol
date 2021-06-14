#version 450
#extension GL_ARB_shading_language_include : require
#include "/defines"
#include "/globals.glsl"

#define BIAS 0.001

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

void main()
{
	uint offset = texelFetch(offsetTexture,ivec2(gl_FragCoord.xy),0).r;

	if (offset == 0)
		discard;

	uint offset2 = texelFetch(offsetTexture2,ivec2(gl_FragCoord.xy),0).r;

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

// #ifdef VISUALIZE_OVERLAPS
// 	// fragColor = vec4(vec3(entryCount2) / maxEntries, 1.0);
// 	fragColor = vec4(vec3(position.w) / 100.0, 1.0);
// 	return;
// #endif

	// Exit just in case (technically should never arrive here because offset would be 0)
	if (entryCount == 0)
		discard;

	vec4 closestPosition;
	vec3 closestNormal;
	// if (position.w < position2.w) {
	// 	closestPosition = position;
	// 	closestNormal = normal.xyz;
	// } else {
	// 	closestPosition = position2;
	// 	closestNormal = normal2.xyz;
	// }

	// Find end positions
    if (65535.0 <= position.w) {
        closestPosition = position2;
        closestNormal = normal2.xyz;
    } else if (65535.0 <= position2.w) {
        closestPosition = position;
        closestNormal = normal.xyz;
    } else {
        closestPosition = mix(position, position2, interpolation);
        closestNormal = mix(normal, normal2, interpolation).xyz;
    }

	// fragColor = vec4(vec3(closestPosition.w / 10.0 / 10.0), 1.0);
	// return;

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

	uint stepCount = 0;
	const uint maxEntryCount = max(entryCount, entryCount2);

	uint sortedCount = 0;
	uint sortedCount2 = 0;

	// sphere tracing parameters
	const uint maximumSteps = 32; // maximum number of steps
	const float eps = 0.0125; // threshold for detected intersection
	const float omega = 1.2; // over-relaxation factor

	const float s = sharpness*sharpnessFactor;

	for (uint currentIndex = 0; currentIndex < maxEntryCount - 1; ++currentIndex) {
		// Increment end index and sort (first list)
		if (currentIndex < entryCount - 1) {
			while (endIndex < entryCount && intersections[indices[endIndex]].near <= intersections[indices[startIndex]].far) {
				uint minimumIndex = endIndex;

				// Find minimum index (based on near distance)
				for(uint i = minimumIndex+1; i < entryCount; i++)
					if(intersections[indices[i]].near < intersections[indices[minimumIndex]].near)
						minimumIndex = i;
				
				// Selection sort swap:
				if (minimumIndex != endIndex)
				{
					uint temp = indices[minimumIndex];
					indices[minimumIndex] = indices[endIndex];
					indices[endIndex] = temp;
				}

				++endIndex;
			}
		}

		// Increment end index and sort (second list)
		if (startIndex < entryCount - 1) {
			while (endIndex2 < entryCount2 && intersections2[indices2[endIndex2]].near <= intersections2[indices2[startIndex2]].far) {
				uint minimumIndex2 = endIndex2;

				// Find minimum index (based on near distance)
				for(uint i = minimumIndex2+1; i < entryCount2; i++)
					if(intersections2[indices2[i]].near < intersections2[indices2[minimumIndex2]].near)
						minimumIndex2 = i;
				
				// Selection sort swap:
				if (minimumIndex2 != endIndex2)
				{
					uint temp2 = indices2[minimumIndex2];
					indices2[minimumIndex2] = indices2[endIndex2];
					indices2[endIndex2] = temp2;
				}

				++endIndex2;
			}
		}

		// Note: At this point both ranges of sphere lists should either have reached an end of the span of overlap,
		// or have reached the end. In both cases it should be safe to continue with sphere tracing.

		// Q: Why +1?
		// A: We (for some reason) check the inner range of the intersecting spheres [start+1, end-1]
		uint ii = indices[startIndex+1];
		uint ii2 = indices2[startIndex2+1];
		float nearDistance = min(intersections[ii].near, intersections2[ii2].near);
		float farDistance = max(intersections[indices[endIndex-1]].far, intersections2[indices2[endIndex2-1]].far);
		// float nearDistance = intersections[ii].near;
		// float farDistance = intersections[indices[endIndex-1]].far;

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
			for (uint j = startIndex; j <= endIndex && j < entryCount; j++)
			{
				uint ij = indices[j];
				uint id = intersections[ij].id;
				uint elementId = bitfieldExtract(id,0,8);

				vec3 aj = intersections[ij].center;
				float rj = intersections[ij].radius; // elements[elementId].radius;
				float weight = intersections[ij].weight;
				// float sphereSharpness = intersections[ij].sharpness;

				vec3 atomOffset = currentPosition.xyz-aj;
				float atomDistance = length(atomOffset)/rj;

				float atomValue = exp(-s*atomDistance*atomDistance) * weight;
				vec3 atomNormal = atomValue*normalize(atomOffset);
				
				sumValue += atomValue;
				sumNormal += atomNormal;
			}
			
			// sum contributions of atoms in the neighborhood (for second surface)
			for (uint j = startIndex2; j <= endIndex2 && j < entryCount2; j++)
			{
				uint ij = indices2[j];
				uint id = intersections2[ij].id;
				uint elementId = bitfieldExtract(id,0,8);

				vec3 aj = intersections2[ij].center;
				float rj = intersections2[ij].radius; // elements[elementId].radius;
				float weight = intersections2[ij].weight;
				// float sphereSharpness = intersections[ij].sharpness;

				vec3 atomOffset = currentPosition.xyz-aj;
				float atomDistance = length(atomOffset)/rj;

				float atomValue = exp(-s*atomDistance*atomDistance) * weight;
				vec3 atomNormal = atomValue*normalize(atomOffset);
				
				sumValue += atomValue;
				sumNormal += atomNormal;
			}
			
			/** -1 because were searching for the distance to the surface when p(x) = 1.0
				* Distance is defined as d(x) = p(x) - t, and as described in paper we estimate
				* t = 1
				*/
			surfaceDistance = sqrt(-log(sumValue) / (s))-1.0;

			if (surfaceDistance < eps)
			{
				if (currentPosition.w <= closestPosition.w)
				{
					closestPosition = currentPosition;
					closestNormal = sumNormal;
				}
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
			if (candidatePosition.w <= closestPosition.w)
			{
				closestPosition = candidatePosition;
				closestNormal = candidateNormal;
			}
		}


		// Increment start indices
		if (currentIndex + 1 < entryCount - 1)
			++startIndex;
		if (currentIndex + 1 < entryCount2 - 1)
			++startIndex2;
	}

	if (closestPosition.w >= 65535.0)
		discard;
	

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