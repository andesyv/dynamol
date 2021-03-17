#version 450

layout(location = 0) in vec3 inPos;

uniform mat4 modelViewProjectionMatrix = mat4(1.0);

out vec3 fragPos;

void main() {
    gl_Position = modelViewProjectionMatrix * vec4(inPos, 1.0);
}