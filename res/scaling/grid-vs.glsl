#version 450

#define GRID_SCALE 1
#define BIAS 0.001

layout(location = 0) in vec3 inPos;

uniform mat4 modelViewProjectionMatrix = mat4(1.0);
uniform mat4 inverseModelViewProjectionMatrix = mat4(1.0);
uniform uint gridScale = 1;
uniform vec3 minb;
uniform vec3 maxb;

struct Cell {
    uvec4 pos;
    uint count;
};
layout(std140, binding = 4) buffer VertexBuffer
{
    Cell cells[];
};

flat out ivec3 gridIndex;
flat out vec3 vPos;

void main() {
    vec3 bdir = (maxb - minb + 2.0 * BIAS).xyz / float(gridScale);

    // Calculate index:
    vec4 p = vec4(inPos, 1.0);
    vPos = p.xyz;
    vec3 dir = p.xyz - minb - BIAS;
    gridIndex = ivec3(dir / bdir);
    uint index = gridIndex.x + gridIndex.y * gridScale + gridIndex.z * gridScale * gridScale;

    // Increment count
    atomicAdd(cells[index].count, 1);

    // Add position
    uvec3 upos = uvec3(floor(p * 1000.0)); // Multiply by 1000 and discretize to int
    // Atomic add position:
    atomicAdd(cells[index].pos.x, upos.x);
    atomicAdd(cells[index].pos.y, upos.y);
    atomicAdd(cells[index].pos.z, upos.z);

    gl_Position = modelViewProjectionMatrix * p;
}
