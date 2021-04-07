#version 450

in vec3 col;

// https://stackoverflow.com/questions/9246100/how-can-i-implement-the-distance-from-a-point-to-a-line-segment-in-glsl
float DistToLine(vec2 pt1, vec2 pt2, vec2 testPt)
{
  vec2 lineDir = pt2 - pt1;
  vec2 perpDir = vec2(lineDir.y, -lineDir.x);
  vec2 dirToPt1 = pt1 - testPt;
  return abs(dot(normalize(perpDir), dirToPt1));
}


out vec4 fragColor;

void main() {
    fragColor = vec4(col, 1.);
}