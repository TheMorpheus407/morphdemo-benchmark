#version 430
layout(location=0) uniform vec4 U[8];
#define TIME U[0].x
#define RES U[0].yz
const float PI = 3.14159265, TAU = 6.2831853;
// integer hashes (exact and identical everywhere)
uint hu(uint x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
float hf(uint x) { return float(hu(x) >> 8) / 16777216.; }
float h1(float x) { return hf(uint(int(x) + 1000000)); }
