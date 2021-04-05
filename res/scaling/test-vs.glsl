#version 450

layout(location = 0) in vec3 position;

out vec3 fragPos_tcs_in;

void main() {
    fragPos_tcs_in = position;
    gl_Position = vec4(position, 1.0);
}