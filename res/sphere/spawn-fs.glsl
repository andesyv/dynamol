#version 450

uniform mat4 modelViewProjectionMatrix;
uniform mat4 inverseModelViewProjectionMatrix;
uniform float interpolation = 0.0;
uniform uint bucketSize = 8u;

in vec4 gFragmentPosition;
flat in vec4 gSpherePosition;
flat in float gSphereRadius;
flat in float gOuterRadius;
flat in float gSharpness;
flat in float gWeight;
flat in uint gSphereId;

layout(binding = 0) uniform sampler2D positionTexture;
layout(binding = 1) uniform sampler2D positionNearTexture;
layout(r32ui, binding = 0) uniform uimage3D offsetImage;

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

layout(binding = 1) uniform atomic_uint count;

layout(std430, binding = 1) buffer intersectionBuffer
{
	BufferEntry intersections[];
};

struct Sphere
{			
	bool hit;
	vec3 near;
	vec3 far;
};
																					
Sphere calcSphereIntersection(float r, vec3 origin, vec3 center, vec3 line)
{
	vec3 oc = origin - center;
	vec3 l = normalize(line);
	float loc = dot(l, oc);
	float under_square_root = loc * loc - dot(oc, oc) + r*r;
	if (under_square_root > 0)
	{
		float da = -loc + sqrt(under_square_root);
		float ds = -loc - sqrt(under_square_root);
		vec3 near = origin+min(da, ds) * l;
		vec3 far = origin+max(da, ds) * l;

		return Sphere(true, near, far);
	}
	else
	{
		return Sphere(false, vec3(0), vec3(0));
	}
}
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
	vec4 fragCoord = gFragmentPosition;
	fragCoord /= fragCoord.w;
	
	vec4 near = inverseModelViewProjectionMatrix*vec4(fragCoord.xy,-1.0,1.0);
	near /= near.w;

	vec4 far = inverseModelViewProjectionMatrix*vec4(fragCoord.xy,1.0,1.0);
	far /= far.w;

	vec3 V = normalize(far.xyz-near.xyz);	
	Sphere sphere = calcSphereIntersection(gSphereRadius, near.xyz, gSpherePosition.xyz, V);
	
	if (!sphere.hit)
		discard;

	vec4 position = texelFetch(positionTexture,ivec2(gl_FragCoord.xy),0);
	BufferEntry entry;
	
	entry.near = length(sphere.near.xyz-near.xyz);
	
	if (entry.near > position.w)
		discard;

	vec4 nearPosition = texelFetch(positionNearTexture,ivec2(gl_FragCoord.xy),0);
	/**
	f(x) = ax + b
	f(x_0) = 0 = a * x_0 + b => a * x_0 = -b
	f(x_1) = 8 = a * x_1 + b => a * x_1 = 8 - b
	a * x_1 = 8 + (a * x_0) => a * (x_1 - x_0) = 8 => a = 8 / (x_1 - x_0)
	a * x_0 = -b => 8 / (x_1 - x_0) * x_0 = -b => b = -(8 * x_0) / (x_1 - x_0)
	f(x) = x * 8 / (x_1 - x_0) - (8 * x_0) / (x_1 - x_0)
		 => (x * 8 - 8 * x_0) / (x_1 - x_0) => 8 * (x - x_0) / (x_1 - x_0) = f(x)
	 */
	uint bucketIndex = clamp(uint(float(bucketSize) * (entry.near - nearPosition.w) / (position.w - nearPosition.w)), 0u, bucketSize - 1u);

	uint index = atomicCounterIncrement(count);
	uint prev = imageAtomicExchange(offsetImage,ivec3(ivec2(gl_FragCoord.xy), bucketIndex),index);

	entry.far = length(sphere.far.xyz-near.xyz);

	entry.center = gSpherePosition.xyz;
	entry.id = gSphereId;
	entry.previous = prev;
	
	entry.radius = gOuterRadius;
	entry.sharpness = gSharpness;
	entry.weight = gWeight;

	intersections[index] = entry;

	discard;
}