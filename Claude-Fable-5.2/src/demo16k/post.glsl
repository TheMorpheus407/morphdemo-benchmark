// depth of field + bloom + tone mapping + grain, and the dot-matrix caption
const int GL[11] = int[](1096767, 18136623, 15255086, 18405233, 18667121, 14815374, 16267326, 32554047, 4329631, 16303663, 0);                 // 5x5 glyphs F R O M N I S E T D space
const int TX[19] = int[](0, 1, 2, 3, 10, 4, 2, 5, 6, 7, 10, 8, 2, 10, 2, 1, 9, 7, 1);   // FROM NOISE TO ORDER
layout(binding=0) uniform sampler2D S;
out vec4 o;
vec3 tone(vec3 x) { return clamp(x * (2.51 * x + .03) / (x * (2.43 * x + .59) + .14), 0., 1.); }
void main() {
  vec2 uv = gl_FragCoord.xy / RES;
  float fo = U[5].z, ap = U[5].w;
  vec4 c0 = textureLod(S, uv, 1.);
  float coc = min(abs(c0.w - fo) / fo * ap, 14.);
  vec3 c = vec3(0); float w = 0.;
  float a = 6.2831853 * hf(uint(gl_FragCoord.x) * 1973U + uint(gl_FragCoord.y) * 9277U + uint(U[0].w) * 26699U);
  for (int i = 0; i < 24; i++) {
    float r = sqrt((float(i) + .5) / 24.);
    vec2 d = r * coc * vec2(cos(a), sin(a)) / RES;
    a += 2.39996;
    vec4 s = textureLod(S, uv + d, max(1., 1. + log2(coc * .3)));
    float cs = min(abs(s.w - fo) / fo * ap, 14.), ws = step(r * coc, max(cs, coc * .5) + 1.);
    c += s.rgb * ws; w += ws;
  }
  c /= max(w, 1.);
  vec2 v = uv - .5, q = (gl_FragCoord.xy * 2. - RES) / RES.y;
  // lens: slight radial colour fringing towards the corners
  vec2 ca = v * dot(v, v) * .008;
  float lod = max(1., 1. + log2(max(coc, 1.) * .6));
  c.r = mix(c.r, textureLod(S, uv + ca, lod).r, .7);
  c.b = mix(c.b, textureLod(S, uv - ca, lod).b, .7);
  vec3 b = vec3(0);
  for (int i = 3; i < 9; i++) b += textureLod(S, uv, float(i)).rgb;
  c += max(b - .5, 0.) * .05 * U[6].y;
  // out-of-focus motes drifting in front of the lens, breathing with the kick
  for (int i = 0; i < 12; i++) {
    uint s = uint(i) * 5U + 77U;
    vec2 m = fract(vec2(hf(s), hf(s + 1U)) + TIME * (vec2(hf(s + 2U), hf(s + 3U)) - .5) * .03) * 2. - 1.;
    float r = .035 + .09 * hf(s + 4U), d = length(q - m * vec2(1.9, 1.1));
    c += vec3(.25, .8, .75) * U[6].y * .022 * smoothstep(r, r * .85, d) * (.5 + .5 * smoothstep(r * .4, r, d)) * (.6 + .4 * sin(TIME * (.4 + hf(s) * .8) + float(i))) * (1. + 2. * exp(-kickAge(TIME) * 5.));
  }
  vec2 tg = ((gl_FragCoord.xy * 2. - RES) / RES.y - vec2(-.678, -.74)) / .012;
  ivec2 ci = ivec2(floor(tg));
  if (ci.x >= 0 && ci.x < 113 && ci.y >= 0 && ci.y < 5 && ci.x % 6 < 5)
    c += vec3(1, .88, .68) * U[7].w * float((GL[TX[ci.x / 6]] >> (ci.x % 6 + 5 * (4 - ci.y))) & 1) * smoothstep(.5, .2, length(fract(tg) - .5));
  c *= U[6].x;                                                 // fade
  c *= 1. - 1.1 * dot(v, v);                                   // vignette
  c = tone(c * 1.15);
  c = pow(c, vec3(1. / 2.2));
  float g = hf(uint(gl_FragCoord.x) * 7919U + uint(gl_FragCoord.y) * 104729U + uint(U[0].w) * 1299709U) - .5;
  c += g * (U[6].z + 1. / 255.);
  o = vec4(c, 1);
}
