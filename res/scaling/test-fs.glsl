#version 450

in vec4 gFragmentPosition;

uniform mat4 modelViewProjectionMatrix;
uniform float interpolation = 0.0;

layout(pixel_center_integer) in vec4 gl_FragCoord;

layout(binding = 0) uniform sampler2D positionTex;
layout(binding = 1) uniform sampler2D positionNearTex;

// https://stackoverflow.com/questions/9246100/how-can-i-implement-the-distance-from-a-point-to-a-line-segment-in-glsl
float DistToLine(vec2 pt1, vec2 pt2, vec2 testPt)
{
    vec2 lineDir = pt2 - pt1;
    vec2 perpDir = vec2(lineDir.y, -lineDir.x);
    vec2 dirToPt1 = pt1 - testPt;
    return abs(dot(normalize(perpDir), dirToPt1));
}

float calcDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}

float LinearizeDepth(float depth) 
{
    const float far = gl_DepthRange.far; 
	const float near = gl_DepthRange.near;
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}


out vec4 fragColor;

void main() {
    vec4 position = texelFetch(positionTex,ivec2(gl_FragCoord.xy),0);
    fragColor = vec4(vec3(1.0 - position.w / 50.0), 1.0);
}