#version 450

in vec4 gFragmentPosition;

layout(pixel_center_integer) in vec4 gl_FragCoord;

layout(binding = 0) uniform sampler2D positionTex;
layout(binding = 1) uniform usampler2D offsetTexture;

// https://stackoverflow.com/questions/9246100/how-can-i-implement-the-distance-from-a-point-to-a-line-segment-in-glsl
float DistToLine(vec2 pt1, vec2 pt2, vec2 testPt)
{
    vec2 lineDir = pt2 - pt1;
    vec2 perpDir = vec2(lineDir.y, -lineDir.x);
    vec2 dirToPt1 = pt1 - testPt;
    return abs(dot(normalize(perpDir), dirToPt1));
}


out vec4 fragColor;

void main() {
	uint index = texelFetch(offsetTexture,ivec2(gl_FragCoord.xy),0).r;
	const vec2 uv = (gFragmentPosition.xy / gFragmentPosition.w) * 0.5 + 0.5;
	fragColor = vec4(vec3(index) / 1000000.0, 1.);
}