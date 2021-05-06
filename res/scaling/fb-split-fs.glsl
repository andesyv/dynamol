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

layout(location = 0) out vec4 frontPosition;
layout(location = 1) out vec4 backPosition;
layout(location = 2) out vec4 frontNormal;
layout(location = 3) out vec4 backNormal;

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

    if (lod1d < lod0d) {
        frontPosition = lod1;
        backPosition = lod0;
        frontNormal = texelFetch(LOD1Normal, ivec2(gl_FragCoord.xy), 0);
        backNormal = texelFetch(LOD0Normal, ivec2(gl_FragCoord.xy), 0);
        gl_FragDepth = lod1d;
    } else {
        frontPosition = lod0;
        backPosition = lod1;
        frontNormal = texelFetch(LOD0Normal, ivec2(gl_FragCoord.xy), 0);
        backNormal = texelFetch(LOD1Normal, ivec2(gl_FragCoord.xy), 0);
        gl_FragDepth = lod0d;
    }
}