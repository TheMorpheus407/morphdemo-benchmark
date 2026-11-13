// 13-tap downsample (U[1].xy = source texel size, U[1].z = 1 for the first pass: firefly-weighted)
layout(binding=0) uniform sampler2D tex;
out vec4 o;
vec3 smp(vec2 uv) { return texture(tex, uv).rgb; }
float kw(vec3 c) { return 1. / (1. + max(c.r, max(c.g, c.b)) * U[1].z); }
void main() {
  vec2 px = U[1].xy, uv = gl_FragCoord.xy * 2. * px;
  vec3 a = smp(uv + px * vec2(-2, -2)), b = smp(uv + px * vec2(0, -2)), c = smp(uv + px * vec2(2, -2));
  vec3 d = smp(uv + px * vec2(-2, 0)), e = smp(uv), f = smp(uv + px * vec2(2, 0));
  vec3 g = smp(uv + px * vec2(-2, 2)), h = smp(uv + px * vec2(0, 2)), i = smp(uv + px * vec2(2, 2));
  vec3 j = smp(uv + px * vec2(-1, -1)), k = smp(uv + px * vec2(1, -1)), l = smp(uv + px * vec2(-1, 1)), m = smp(uv + px * vec2(1, 1));
  vec3 g0 = (j + k + l + m) * .25, g1 = (a + b + d + e) * .25, g2 = (b + c + e + f) * .25, g3 = (d + e + g + h) * .25, g4 = (e + f + h + i) * .25;
  float w0 = kw(g0) * .5, w1 = kw(g1) * .125, w2 = kw(g2) * .125, w3 = kw(g3) * .125, w4 = kw(g4) * .125;
  o = vec4((g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4), 1);
}
