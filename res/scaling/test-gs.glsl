#version 450

uniform uint INVOCATIONS = 3;

layout(points) in;
layout(invocations = INVOCATIONS) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 fragPos_tcs_in[];
out flat uint triangleID;

void main() {
    vec4 center = gl_in[0].gl_Position + vec4(float(gl_InvocationID) / (INVOCATIONS - 1) - 0.5, 0., 0., 0.);
    triangleID = gl_InvocationID + 1;
    gl_Position = center + vec4(-0.1, -0.1, 0., 0.);
    EmitVertex();
    gl_Position = center + vec4(0., 0.1, 0., 0.);
    EmitVertex();
    gl_Position = center + vec4(0.1, -0.1, 0., 0.);
    EmitVertex();


    EndPrimitive();
}