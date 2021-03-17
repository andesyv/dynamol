#version 450

in vec3 fragPos;
in vec2 uv;

uniform int posCount = 0;
uniform mat4 inverseModelViewProjectionMatrix;
uniform mat4 inverseModelMatrix;
uniform mat4 modelViewProjectionMatrix;

layout(binding = 0) uniform sampler2D posBuffer;
layout(binding = 1) uniform usampler2D countBuffer;

layout(std140, binding = 4) buffer VertexBuffer
{
    vec4 pos[];
};

float linearizeDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}

float sdf(vec3 p) {
    float d = 1000.0;
    for (int i = 0; i < 10 && i < posCount; ++i) {
        d = min(d, length(p - pos[i].xyz) - 1.0);
    }
    
    return d;
}

out vec4 fragColor;

void main() {
    // near = MVP^-1 * rd
    vec4 near = inverseModelViewProjectionMatrix*vec4(uv,-1.0,1.0);
	near /= near.w;

	vec4 far = inverseModelViewProjectionMatrix*vec4(uv,1.0,1.0);
	far /= far.w;

    vec3 ro = near.xyz;
    float sceneDepth = length((far - near).xyz);
    vec3 rd = (far - near).xyz / sceneDepth;

    // // Basic sphere tracing:
    // float d = 0.;
    // for (int i = 0; i < 100; ++i) {
    //     vec3 p = ro + d * rd;
    //     float dist = sdf(p);
    //     if (dist < 0.1) {
    //         float depth = linearizeDepth(p);
    //         fragColor = vec4(vec3(depth), 1.);
    //         gl_FragDepth = depth;
    //         return;
    //     }

    //     d += dist;
    // }

    // discard;
    vec2 texCoord = uv * 0.5 + 0.5;
    fragColor = vec4(texture(posBuffer, texCoord).rgb, 1.0);
}