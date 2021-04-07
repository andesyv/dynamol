#version 450

layout(location = 0) in uvec4 inPos;
layout(location = 1) in uvec4 inCount;

uniform mat4 modelViewProjectionMatrix = mat4(1.0);

layout(std140, binding = 5) buffer VertexGenerated
{
    vec4 positions[];
};

flat out uint index;

void main() {
    vec4 pos = vec4(vec3(inPos.xyz) / float(inCount.x * 1000), 0.);
    // vec4 pos = vec4(vec3(inPos.xyz) / 1000000.0, 1.0);
    uint id = inPos.w;
    pos.w = uintBitsToFloat(id);

    index = gl_VertexID;

    if (0 < inCount.x) {
        positions[gl_VertexID] = pos;
        
        gl_Position = modelViewProjectionMatrix * vec4(pos.xyz, 1.0);
    }
}