#version 450

flat in ivec3 gridIndex;
flat in vec3 vPos;
flat in uint index;

uniform uint gridScale = 1;

out vec4 fragColor;

void main() {
    discard;
    // float gScale = float(gridScale);
    // uint index = gridIndex.x + gridIndex.y * gridScale + gridIndex.z * gridScale * gridScale;
    // vec3 coord = vec3(float(gridIndex.x) / gScale, float(gridIndex.y) / gScale, float(gridIndex.z) / gScale);
    // uvec3 poss = uvec3(floor(vec3(vPos) * 1000.0)) * 10000;
    // // fragColor = vec4(vec3(float(index) / (gScale * gScale * gScale)), 1.0);
    // fragColor = vec4(vec3(poss) / (1000000.0 * 10000), 1.0);
}