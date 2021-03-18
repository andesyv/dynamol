#version 450

#define GRID_SCALE 1

in vec3 fragPos;
in vec2 uv;

uniform int posCount = 0;
uniform mat4 inverseModelViewProjectionMatrix;
uniform mat4 inverseModelMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform uint gridScale = 1;

struct Cell {
    uvec4 pos;
    uint count;
};
layout(std140, binding = 4) buffer VertexBuffer
{
    Cell cells[];
};

out vec4 fragColor;

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
    for (int i = 0; i < gridScale; ++i) {
        if (cells[i].count == 0)
            continue;

        vec3 fpos = vec3(cells[i].pos.xyz) * 0.001; // Convert to floating points (divide by 1000)
        vec3 avgpos = fpos / float(cells[i].count);
        d = min(d, length(p - avgpos) - 10.0);
    }
    
    return d;
}

void main() {
    // near = MVP^-1 * rd
    vec4 near = inverseModelViewProjectionMatrix*vec4(uv,-1.0,1.0);
	near /= near.w;

	vec4 far = inverseModelViewProjectionMatrix*vec4(uv,1.0,1.0);
	far /= far.w;

    vec3 ro = near.xyz;
    float sceneDepth = length((far - near).xyz);
    vec3 rd = (far - near).xyz / sceneDepth;

    // Basic sphere tracing:
    float d = 0.;
    for (int i = 0; i < 100; ++i) {
        vec3 p = ro + d * rd;
        float dist = sdf(p);
        if (dist < 0.1) {
            float depth = linearizeDepth(p);
            fragColor = vec4(vec3(depth), 1.);
            gl_FragDepth = depth;
            return;
        }

        d += dist;
    }
    
    discard;
    // fragColor = vec4(abs(vec3(cells[0].pos.xyz) / (float(cells[0].count) * 1000.0)), 1.0);
}