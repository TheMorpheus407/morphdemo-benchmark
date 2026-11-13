// ---- captions: dot-matrix text drawn additively into the HDR buffer -----------------
out vec4 o;
// one dot of glyph g at cell c (x 0..4 from the left, y 0..6 from the top)
float gdot(int g, ivec2 c) {
  uint b = c.y < 6 ? FONT[g * 2] >> uint(c.y * 5 + 4 - c.x) : FONT[g * 2 + 1] >> uint(4 - c.x);
  return float(b & 1U);
}
// string s with its top-left corner at the origin, p in dot units; the characters "decode"
// from random glyphs: character i settles at local time i * .07 + .25 (tl = seconds since it appeared)
float text(vec2 p, int s, float tl) {
  ivec2 st = STR[s];
  if (p.x < 0. || p.y < 0. || p.y >= 7. || p.x >= float(st.y * 6) || tl < 0.) return 0.;
  int ci = int(p.x) / 6, cx = int(p.x) - ci * 6;
  if (cx > 4) return 0.;
  int g = TXT[st.x + ci];
  float ts = tl - float(ci) * .07;
  if (ts < 0.) return 0.;
  if (ts < .25) g = int(hf(uint(ci) * 31U + uint(tl * 24.) * 7U + uint(s) * 977U) * float(NGLYPH - 5));
  vec2 f = fract(p) - .5;
  return gdot(g, ivec2(cx, int(p.y))) * smoothstep(.45, .28, length(f));
}
void main() {
  float t = TIME;
  vec2 px = vec2(gl_FragCoord.x, RES.y - gl_FragCoord.y);     // pixels, origin top-left
  vec3 c = vec3(0);
  float al = 0.;
  // chapter labels, bottom left
  for (int i = 0; i < 5; i++) {
    float t0 = i == 0 ? .8 : i == 1 ? T_FLOW : i == 2 ? T_GRAV : i == 3 ? T_RES : T_LAT;
    float t1 = i == 0 ? T_FLOW : i == 1 ? T_GRAV : i == 2 ? T_RES : i == 3 ? T_LAT : T_MORPH;
    float tl = t - t0 - .15;
    if (tl < 0. || t > t1 + .1) continue;
    float a = (1. - smoothstep(t1 - .6, t1 - .1, t)) * (.6 + .4 * exp(-tl * .8));
    vec2 q = (px - vec2(96., RES.y - 112.)) / 4.;
    c += vec3(.8, .9, 1.) * text(q, i, tl) * a * (i == 0 ? 3. : .9);
    // dark plate behind the label so that it stays readable over the static
    vec2 b = abs(q - vec2(float(STR[i].y) * 3. - .5, 3.5)) - vec2(float(STR[i].y) * 3. + 2., 6.);
    al = max(al, smoothstep(1., -1., max(b.x, b.y)) * .93 * a * smoothstep(0., .2, tl));
  }
  // entropy read-out, top right: label + three digits that jitter less and less
  {
    float tl = t - 1.2, a = (1. - smoothstep(T_OUT - .05, T_OUT + .1, t)) * .75;
    vec2 q = (px - vec2(RES.x - 96. - 13. * 24., 84.)) / 4.;
    float en = entropy(t);
    en = clamp(en + (hf(uint(t * 12.)) - .5) * .03 * en, 0., .999);
    a *= 1. + 2.5 * (1. - smoothstep(6., 8.5, t));          // brighter while it has to stand against the static
    c += vec3(.8, .9, 1.) * text(q, 6, tl) * a;
    vec2 p = q - vec2(54., 0.);
    if (tl > .6 && p.x >= 0. && p.x < 24. && p.y >= 0. && p.y < 7.) {
      int ci = int(p.x) / 6, cx = int(p.x) - ci * 6;
      int dg = ci == 0 ? 0 : ci == 2 ? int(en * 10.) : int(mod(en * 100., 10.));
      int g = ci == 1 ? 38 : 26 + dg;
      if (cx < 5) c += vec3(1., .85, .6) * gdot(g, ivec2(cx, int(p.y))) * smoothstep(.45, .28, length(fract(p) - .5)) * a;
    }
    vec2 b = abs(q - vec2(39., 3.5)) - vec2(41., 6.);
    al = max(al, smoothstep(1., -1., max(b.x, b.y)) * .93 * min(a, 1.) * (1. - smoothstep(7., 9., t)) * smoothstep(0., .2, tl));
  }
  // subtitle under the word
  {
    float tl = t - (T_MORPH + 3.2);
    float a = 1. - smoothstep(T_OUT - .05, T_OUT + .1, t);
    c += vec3(1., .85, .6) * text((px - vec2(RES.x * .5 - 19. * 6. * 2.5 + 2.5, RES.y * .5 + 290.)) / 5., 5, tl) * a * .9;
  }
  o = vec4(c, al);
}
