#version 450

#define GRID_SCALE 1

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

void main() {
    vec4 b1 = inverseModelViewProjectionMatrix * vec4(minb, 1.0);
    b1 /= b1.w;

    vec4 b2 = inverseModelViewProjectionMatrix * vec4(maxb, 1.0);
    b2 /= b2.w;

    vec3 bdir = (b2 - b1).xyz / GRID_SCALE;

    // Calculate index:
    vec4 p = vec4(inPos, 1.0);
    vec3 dir = p.xyz - minb;
    ivec3 gridIndex = ivec3(dir / bdir);
    uint index = gridIndex.x + gridIndex.y * gridScale + gridIndex.z * gridScale * gridScale;

    // Add position
    uvec3 upos = uvec3(p * 1000.0); // Multiply by 1000 and discretize to int
    // Atomic add position:
    atomicAdd(cells[index].pos.x, upos.x);
    atomicAdd(cells[index].pos.y, upos.y);
    atomicAdd(cells[index].pos.z, upos.z);

    // Increment count
    atomicAdd(cells[index].count, 1);
}
