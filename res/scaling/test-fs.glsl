#version 450

in vec4 gFragmentPosition;
// in vec3 fragPos_fs;
// in flat vec2 trianglePos[3];

// https://stackoverflow.com/questions/9246100/how-can-i-implement-the-distance-from-a-point-to-a-line-segment-in-glsl
float DistToLine(vec2 pt1, vec2 pt2, vec2 testPt)
{
  vec2 lineDir = pt2 - pt1;
  vec2 perpDir = vec2(lineDir.y, -lineDir.x);
  vec2 dirToPt1 = pt1 - testPt;
  return abs(dot(normalize(perpDir), dirToPt1));
}

layout(binding = 4) uniform atomic_uint redrawCount;


out vec4 fragColor;

void main() {
    // float dist = 1000.0;
    // dist = min(dist, DistToLine(trianglePos[0], trianglePos[1], fragPos_fs.xy));
    // dist = min(dist, DistToLine(trianglePos[1], trianglePos[2], fragPos_fs.xy));
    // dist = min(dist, DistToLine(trianglePos[2], trianglePos[0], fragPos_fs.xy));

    // fragColor = vec4(vec3(pow(1.0 - dist, 20.0)), 1.);

    uint count = atomicCounter(redrawCount);
    fragColor = vec4(vec3(float(count) / 100.0), 1.);

    // fragColor = vec4(fragPos_es.xy, 0., 1.);
    // fragColor = vec4(gl_FragCoord.xy / 1000.0, 0., 1.0);
}