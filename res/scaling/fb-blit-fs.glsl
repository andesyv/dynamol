#version 450

in vec4 gFragmentPosition;
layout(pixel_center_integer) in vec4 gl_FragCoord;

layout(binding = 0) uniform sampler2D LOD0Position;
layout(binding = 1) uniform sampler2D LOD0Normal;
layout(binding = 2) uniform sampler2D LOD0PositionNear;

layout(location = 0) out vec4 middlePosition;
layout(location = 1) out vec4 middleNormal;
layout(location = 2) out vec4 middlePositionNear;

void main() {
    vec4 lod0 = texelFetch(LOD0Position,ivec2(gl_FragCoord.xy),0);
    vec4 lod0n = texelFetch(LOD0Normal,ivec2(gl_FragCoord.xy),0);
    vec4 lod0near = texelFetch(LOD0PositionNear,ivec2(gl_FragCoord.xy),0);

    middlePosition = lod0;
    middleNormal = lod0n;
    middlePositionNear = lod0near;
}