#version 450

flat in uint index;

struct Cell
{
	uvec4 pos;
	uvec4 count;
};

layout(std430, binding = 7) buffer GridBuffer
{
    Cell cells[];
};

out vec4 fragColor;

void main() {
    discard;
    // float gScale = float(gridScale);
    // uint index = gridIndex.x + gridIndex.y * gridScale + gridIndex.z * gridScale * gridScale;
    // vec3 coord = vec3(float(gridIndex.x) / gScale, float(gridIndex.y) / gScale, float(gridIndex.z) / gScale);
    // uvec3 poss = uvec3(floor(vec3(vPos) * 1000.0)) * 10000;
    // // fragColor = vec4(vec3(float(index) / (gScale * gScale * gScale)), 1.0);
    vec4 pos = vec4(vec3(cells[index].pos.xyz) / float(cells[index].count.x * 1000), 0.);
    fragColor = vec4(vec3(index) / 100000000.0, 1.0);
}