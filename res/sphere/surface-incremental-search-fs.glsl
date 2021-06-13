#version 450
#extension GL_ARB_shading_language_include : require
#include "/defines"

#define BIAS 0.001
#define EPSILON 0.001
#define MAX_ENTRIES 128
#define MAX_TRACE_ITERATIONS 1
#define MAX_ITERATIONS 10

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

// Does one selective sort for index in range [index, entries> and returns new (assumed) sorted length
uint selectiveSort(uint index, in uint indices[MAX_ENTRIES], uint entryCount) {
	uint minimalIndex = index;
	uint current = index + 1;
	// Find smallest in [endIndex, end]
	for (; current < entryCount; ++current)
		if (intersections[indices[current]].near < intersections[indices[minimalIndex]].near)
			minimalIndex = current;
	
	if (index != minimalIndex)
		swap(indices[index], indices[minimalIndex]);
	
	// Mark range up to index + 1 as sorted
	return index + 1;
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

	uint entryCount = 0;
	uint entryCount2 = 0;
	uint indices[MAX_ENTRIES];
	uint indices2[MAX_ENTRIES];

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
// 	// fragColor = vec4(vec3(entryCount2) / MAX_ENTRIES, 1.0);
// 	fragColor = vec4(vec3(position.w) / 100.0, 1.0);
// 	return;
// #endif

	// Exit just in case (technically should never arrive here because offset would be 0)
	if (entryCount == 0)
		discard;

	vec4 closestPosition = position;
	vec3 closestNormal = normal.xyz;

	// fragColor = vec4(vec3(closestPosition.w / 10.0 / 10.0), 1.0);
	// return;

	float sharpnessFactor = 1.0;
	vec3 ambientColor = ambientMaterial;
	vec3 diffuseColor = vec3(1.0,1.0,1.0);
	vec3 specularColor = specularMaterial;
	vec3 diffuseSphereColor = vec3(1.0,1.0,1.0);

	vec4 rayOrigin = vec4(near.xyz, 0.0);
	vec4 rayDir = vec4(V, 1.0);

	uint minIndex = 0;
	// Keep hold of how far we've sorted thus far.
	// (Pre-emptively sort first index):
	uint sortedCount = selectiveSort(0, indices, entryCount);

	float traceNear = intersections[indices[0]].near;
	float traceFar = closestPosition.w;

	float t = traceNear;
	vec4 p = rayOrigin + t * rayDir;
	vec3 n = closestNormal;
	const float s = sharpness*sharpnessFactor;

	for (uint traceCount = 0; traceCount < MAX_TRACE_ITERATIONS && t < traceFar; ++traceCount) {
		// Increment startIndex if already past intersecting sphere
		/** Note: At first iteration minIndex isn't the minimal sphere yet but
		 * since no spheres behind the camera are added to the intersection list,
		 * no sphere can have a far distance smaller than 0, which t starts on.
	 	 */
		// while (intersections[indices[minIndex]].far < t)
		// 	++minIndex;

		float minDistance = position.w - t;

		uint startIndex = minIndex;
		uint endIndex = startIndex;

		for (uint i = 0; i < MAX_ITERATIONS; ++i) {
			// Extend range of spheres while until indices no longer intersect
			while (intersections[indices[endIndex]].near < intersections[indices[startIndex]].far) {
				++endIndex;

				// Selection sort (up to endIndex)
				if (sortedCount < endIndex+1)
					sortedCount = selectiveSort(endIndex, indices, entryCount);
			}


			// Sum up contributions:
			float sumValue = 0.0;
			vec3 sumNormal = vec3(0.0);

			for (uint j = startIndex; j < endIndex-1; ++j) {
				uint ij = indices[j];
				uint id = intersections[ij].id;
				uint elementId = bitfieldExtract(id,0,8);

				vec3 aj = intersections[ij].center;
				float rj = intersections[ij].radius; // elements[elementId].radius;
				float weight = intersections[ij].weight;
				// float sphereSharpness = intersections[ij].sharpness;

				vec3 atomOffset = p.xyz-aj;
				float atomDistance = length(atomOffset)/rj;

				float atomValue = exp(-s*atomDistance*atomDistance) * weight;
				vec3 atomNormal = atomValue*normalize(atomOffset);
				
				sumValue += atomValue;
				sumNormal += atomNormal;
			}
			float surfaceDistance = sqrt(-log(sumValue) / (s))-1.0;

			// Save smallest distance found
			if (surfaceDistance < minDistance) {
				minDistance = surfaceDistance;
				n = sumNormal;
			}

			// Terminate early if surface found
			if (surfaceDistance < EPSILON)
				break;
			
			++startIndex;
		}

		// Increment by smallest distance
		t += minDistance;
		p = rayOrigin + t * rayDir;
	}

	if (p.w < position.w) {
		closestPosition = p;
		closestNormal = n;
	}

	if (closestPosition.w >= 65535.0)
		discard;


#ifdef VISUALIZE_OVERLAPS
	fragColor = vec4(vec3(closestPosition.w / 100.0), 1.0);
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