#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

out flat vec2 trianglePos[3];
out vec3 fragPos_fs;

void main() {
    for (uint i=0; i < 3; ++i) {
        trianglePos[i] = gl_in[i].gl_Position.xy;
    }

    gl_Position = gl_in[0].gl_Position;
    fragPos_fs = gl_in[0].gl_Position.xyz;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    fragPos_fs = gl_in[1].gl_Position.xyz;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    fragPos_fs = gl_in[2].gl_Position.xyz;
    EmitVertex();
    EndPrimitive();
}