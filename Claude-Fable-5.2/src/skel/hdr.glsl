#version 430
layout(location=0) uniform vec4 U[8];
#define TIME U[0].x
#define RES U[0].yz
const float BPM = 120.;
float hash(float n) { return fract(sin(n * 12.9898) * 43758.5453); }
