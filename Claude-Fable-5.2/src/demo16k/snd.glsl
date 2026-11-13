// The soundtrack: one pixel = one sample frame (48 kHz).  o0 = (dry L, dry R, reverb send L, R), o1 = (delay send L, R, -, -)
layout(location=0) out vec4 o0;
layout(location=1) out vec4 o1;
// oscillator with exact phase: absolute sample index times an integer phase increment (wraps mod 2^32)
float osc(uint n, float f) { return sin(float(n * uint(f * 89478.485)) * 1.4629181e-9); }
float mtof(float m) { return 440. * exp2((m - 69.) / 12.); }
float nz(uint n) { return hf(n) * 2. - 1.; }
vec2 pn(float x) { return vec2(cos(x * 1.5708), sin(x * 1.5708)); }
// one windowed pad instance for half-bar h: additive, partials start scattered / stretched and converge to harmonics
vec2 pad(uint n, int h) {
  ivec4 c = chord(h);
  float bt = float(h) * .5, q = clamp(bt / 6., 0., 1.);                 // order of the tuning
  float sp = 5. * (1. - q) * (1. - q), st = .06 * (1. - q), br = mix(1.9, 1., clamp((bt - 5.) / 11., 0., 1.));
  vec2 a = vec2(0);
  for (int j = 0; j < 4; j++) {
    float f0 = mtof(float(c[j]));
    for (int k = 1; k < 17; k++) {
      float r = hf(uint(j * 16 + k) * 77U + 5U) - .5, f = f0 * pow(float(k), 1. + st) * exp2(r * sp / 12.), g = pow(float(k), -br);
      a += g * vec2(osc(n, f * 1.0028), osc(n, f * .9972)) + g * .5 * osc(n, f);
    }
  }
  return a * .25;
}
void main() {
  int n = int(gl_FragCoord.x) + int(gl_FragCoord.y) * 2048, i = n / 7500, b = i >> 4;
  uint un = uint(n);
  float t = float(n) / 48000., bt = t / BAR;
  vec2 dry = vec2(0), rev = vec2(0), del = vec2(0);

  // --- pad / drone: overlap-add of two half-bar instances
  float x = float(n % 60000) / 60000., w = .5 - .5 * cos(PI * x);
  int h = n / 60000;
  vec2 p = pad(un, h) * w + pad(un, h - 1) * (1. - w);
  float pl = smoothstep(0., 3., t) * mix(.75, 1., smoothstep(12., 16., t)) * (1. + .25 * smoothstep(37.5, 40., t));
  p *= pl * .28 * (1. - .2 * smoothstep(50., 52., t));
  // --- kick, with a pumping gain for pad and bass
  int ik = kickSlot(i);
  float ka = float(n - ik * 7500) / 48000., kl = b < 6 ? .55 : 1.;
  float kph = 43. * ka + 3.2 * (1. - exp(-ka / .035));
  float kick = sin(TAU * kph) * exp(-ka * 6.) * (1. - exp(-ka * 1800.)) * kl * .5;
  float duck = 1. - .65 * exp(-ka * 7.) * kl;
  dry += kick + p * duck * .55;
  rev += p * duck * .6;
  // --- sub bass: chord root, exact phase, short crossfade at the bar line
  if (b > 5) {
    float bx = float(n % 120000) / 48000., bw = smoothstep(0., .04, bx);
    int bb = n / 120000;
    float f1 = mtof(float(BS[CB[min(bb, 23)]])), f0 = mtof(float(BS[CB[min(max(bb - 1, 6), 23)]]));
    float s1 = osc(un, f1) + .5 * osc(un, f1 * 2.) + .25 * osc(un, f1 * 3.), s0 = osc(un, f0) + .5 * osc(un, f0 * 2.) + .25 * osc(un, f0 * 3.);
    dry += mix(s0, s1, bw) * .12 * duck * smoothstep(15., 15.3, t) * (1. - smoothstep(55., 58., t));
  }
  // --- droplets / arpeggio (the notes that seed the culture)
  for (int j = 0; j < 9; j++) {
    vec4 d = drop(i - j + 1);
    float a = float(n - int(d.x * 48000.)) / 48000.;
    if (d.x >= 0. && a >= 0.) {
      float f = mtof(d.y), q = clamp(float(i - j) / 16. / 9., 0., 1.);
      float ph = f * a - f * .4 * .01 * (1. - exp(-a / .01));
      float e = exp(-a * mix(4.5, 6.5, q)) * (1. - exp(-a * 700.));
      float s = sin(TAU * ph + (.5 + 1.2 * exp(-a * 10.)) * sin(TAU * ph * mix(2.76, 2., q))) + .3 * sin(TAU * ph * mix(4.31, 4., q)) * exp(-a * 9.);
      vec2 v = s * e * pn(.15 + .7 * d.z / 16.) * .26 * (b > 9 && b < 20 ? .8 : 1.);
      dry += v; rev += v * .5; del += v * .6;
    }
  }
  // --- lead line over the resolution (bars 16..20), a little tune in D major
  for (int j = 0; j < 12; j++) {
    vec3 m = lead(j);
    float a = t - 40. - m.x * BEAT, l = m.y * BEAT;
    if (a > 0. && a < l + .6) {
      float f = mtof(m.z), ph = f * a - f * .0002 * cos(TAU * 5.2 * a) * smoothstep(.15, .5, a);
      float e = (1. - exp(-a * 40.)) * exp(-max(a - l, 0.) * 7.) * (j > 9 ? .55 : 1.);
      vec2 v = (sin(TAU * ph) + .35 * sin(TAU * 2. * ph) + .12 * sin(TAU * 3. * ph)) * e * .075 * vec2(.9, 1.1);
      dry += v; rev += v * .8; del += v * .5;
    }
  }
  // --- noise: hiss bed (low-passed white noise) that thins out, and crackle
  float lp = 0., al = mix(.08, .35, .5 + .5 * sin(t * .7));
  for (int j = 0; j < 40; j++) lp += nz(un + uint(j)) * exp(-float(j) * al);
  lp *= al * 2.2;
  float nl = .09 * smoothstep(0., 2., t) * (1. - smoothstep(8., 20., t));
  dry += lp * nl * vec2(.8 + .2 * sin(t * 1.3), .8 - .2 * sin(t * 1.3)); rev += lp * nl * .5;
  int cs = n / 192; float ca = float(n % 192) / 48000., cr = hf(uint(cs) * 3U + 9U);
  if (cr < .14 * (1. - smoothstep(1., 14., t)) + .004) {
    vec2 v = sin(TAU * ca * (2500. + 6000. * hf(uint(cs) * 3U + 10U))) * exp(-ca * 1200.) * pn(hf(uint(cs) * 3U + 11U)) * .13 * smoothstep(0., 1., t);
    dry += v; rev += v * .6;
  }
  // --- hats (soft off-beats from bar 6, full pattern in bars 10..19) and rim
  if (b > 5 && b < 20) {
    int s = i & 15;
    float a = float(n % 7500) / 48000., hp = nz(un) - nz(un + 2U);
    float hl = (s & 3) == 2 ? (b > 9 ? 1. : .5) : b < 10 ? 0. : (s & 1) == 1 ? .35 : .15;
    dry += hp * exp(-a * (s == 14 ? 25. : 80.)) * hl * .12 * vec2(.7, 1.);
    if ((s & 7) == 4 && b > 9) {
      float rim = (sin(TAU * 196. * a) + .6 * sin(TAU * 331. * a)) * exp(-a * 45.) * .5 + lp * exp(-a * 30.) * .9 + hp * exp(-a * 60.) * .1;
      dry += rim * .2; rev += rim * .15;
    }
  }
  // --- riser into the resolution (bar 15) and impacts at bar 16 and bar 20
  float rz = clamp((t - 37.5) / 2.5, 0., 1.) * step(t, 40.);
  dry += (nz(un) - nz(un + 1U)) * rz * rz * rz * .06; rev += lp * rz * rz * .12;
  for (int j = 0; j < 2; j++) {
    float a = t - (j == 0 ? 40. : 50.);
    if (a >= 0.) {
      float bm = sin(TAU * (36. * a + 1.6 * (1. - exp(-a / .06)))) * exp(-a * 2.2) * .5;
      dry += bm; rev += (lp * exp(-a * 3.) * .5 + nz(un) * exp(-a * 9.) * .1) * vec2(1, -1) + bm * .1;
    }
  }
  float g = 1. - smoothstep(56.5, 59.3, t);
  o0 = vec4(dry, rev) * g;
  o1 = vec4(del * g, 0, 0);
}
