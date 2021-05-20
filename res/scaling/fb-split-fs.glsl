#version 450

in vec4 gFragmentPosition;
layout(pixel_center_integer) in vec4 gl_FragCoord;

layout(binding = 0) uniform sampler2D LOD0Position;
layout(binding = 1) uniform sampler2D LOD1Position;

layout(binding = 2) uniform sampler2D LOD0Normal;
layout(binding = 3) uniform sampler2D LOD1Normal;

layout(binding = 4) uniform sampler2D LOD0Depth;
layout(binding = 5) uniform sampler2D LOD1Depth;

uniform mat4 modelViewProjectionMatrix;
uniform float interpolation = 0.0;

layout(location = 0) out vec4 frontPosition;
layout(location = 1) out vec4 backPosition;
layout(location = 2) out vec4 frontNormal;
layout(location = 3) out vec4 backNormal;
layout(location = 4) out vec4 middlePosition;
layout(location = 5) out vec4 middleNormal;

float calcDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}

void main() {
    vec4 lod0 = texelFetch(LOD0Position,ivec2(gl_FragCoord.xy),0);
    vec4 lod1 = texelFetch(LOD1Position,ivec2(gl_FragCoord.xy),0);

    float lod0d = texelFetch(LOD0Depth,ivec2(gl_FragCoord.xy),0).r;
    float lod1d = texelFetch(LOD1Depth,ivec2(gl_FragCoord.xy),0).r;

    vec4 lod0n = texelFetch(LOD0Normal,ivec2(gl_FragCoord.xy),0);
    vec4 lod1n = texelFetch(LOD1Normal,ivec2(gl_FragCoord.xy),0);

    vec4 front;
    vec4 back;

    if (lod1.w < lod0.w) {
        front = lod1;
        back = lod0;
        frontNormal = lod1n;
        backNormal = lod0n;
        gl_FragDepth = lod1d;
    } else {
        front = lod0;
        back = lod1;
        frontNormal = lod0n;
        backNormal = lod1n;
        gl_FragDepth = lod0d;
    }

    frontPosition = front;
    backPosition = back;

    middlePosition = mix(lod0, lod1, interpolation);
    middleNormal = mix(lod0n, lod1n, interpolation);
}