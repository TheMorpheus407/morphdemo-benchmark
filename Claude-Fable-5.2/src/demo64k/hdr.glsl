#version 430
layout(location=0) uniform vec4 U[8];
#define TIME U[0].x
#define RES U[0].yz
#define PI 3.14159265
#define TAU 6.2831853

// ---- hashing ---------------------------------------------------------------
uint hu(uint x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
float hf(uint x) { return float(hu(x) >> 8) * (1. / 16777216.); }
vec2 hf2(uint x) { uint a = hu(x); return vec2(float(a >> 8), float(hu(a ^ 0x9e3779b9U) >> 8)) * (1. / 16777216.); }
vec3 hf3(uint x) { uint a = hu(x), b = hu(a ^ 0x9e3779b9U); return vec3(float(a >> 8), float(b >> 8), float(hu(b ^ 0x85ebca6bU) >> 8)) * (1. / 16777216.); }
float hl(ivec3 p) { return hf(uint(p.x) * 73856093U ^ uint(p.y) * 19349663U ^ uint(p.z) * 83492791U); }

// value noise with analytic gradient: x = value in [0,1], yzw = d/dp
vec4 noised(vec3 x) {
  vec3 fl = floor(x), w = x - fl;
  ivec3 i = ivec3(fl);
  vec3 u = w * w * w * (w * (w * 6. - 15.) + 10.), du = 30. * w * w * (w * (w - 2.) + 1.);
  float a = hl(i), b = hl(i + ivec3(1, 0, 0)), c = hl(i + ivec3(0, 1, 0)), d = hl(i + ivec3(1, 1, 0));
  float e = hl(i + ivec3(0, 0, 1)), f = hl(i + ivec3(1, 0, 1)), g = hl(i + ivec3(0, 1, 1)), h = hl(i + ivec3(1, 1, 1));
  float k1 = b - a, k2 = c - a, k3 = e - a, k4 = a - b - c + d, k5 = a - c - e + g, k6 = a - b - e + f, k7 = -a + b + c - d + e - f - g + h;
  return vec4(a + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z,
              du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z, k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x, k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y));
}
// fbm potential and its gradient (3 octaves, domain rotated between octaves)
vec4 fbmd(vec3 p, float gain) {
  const mat3 R = mat3(0., .8, .6, -.8, .36, -.48, -.6, -.48, .64);
  mat3 m = mat3(1);
  vec4 s = vec4(0);
  float a = .5;
  p = R * p;
  m = R;
  for (int i = 0; i < 3; i++) {
    vec4 n = noised(p);
    s += vec4(n.x * a, a * (n.yzw * m));
    a *= gain;
    p = R * p * 2. + 17.31;
    m = 2. * R * m;
  }
  return s;
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float sat(float x) { return clamp(x, 0., 1.); }
float sq(float x) { return x * x; }
