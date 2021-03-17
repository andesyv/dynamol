#version 450

in vec3 atomPos;

// Multiple Render Targets:
layout(location = 0) out vec3 pos;
layout(location = 1) out uint count;

void main() {
    pos += atomPos;
    ++count;
}