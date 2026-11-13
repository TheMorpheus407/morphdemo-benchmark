// ---- soundtrack: every sample is a pure function of its index ---------------------
// outputs four stereo stems: o0.rg dry, o0.ba reverb send, o1.rg delay send, o1.ba hall send
layout(location=0) out vec4 o0;
layout(location=1) out vec4 o1;

const int SR = 48000, STEP = 6000, BARN = 96000;   // samples per second / 16th / bar

float nz(uint n) { return hf(n) * 2. - 1.; }
float mtof(float m) { return 440. * exp2((m - 69.) / 12.); }
// exact phase (cycles) of a constant frequency f after k samples
float ph(int k, float f) { return float(uint(k) * uint(f * 89478.485)) * (1. / 4294967296.); }
float sn(int k, float f) { return sin(TAU * ph(k, f)); }
// band-limited bright oscillator: sum of a^h sin(h x); a = brightness (0..1), peak normalised
float dsf(int k, float f, float a) {
  a = min(a, exp(-5.8 * f / 24000.));
  float th = TAU * ph(k, f);
  return (1. - a * a) * sin(th) / (1. + a * a - 2. * a * cos(th));
}
float env(float t, float a, float d) { return smoothstep(0., a, t) * exp(-t * d); }
vec2 pan(float x, float p) { return x * vec2(cos(p * 1.5708), sin(p * 1.5708)) * 1.4142; }

// --- drums ---------------------------------------------------------------------------
float kick(float t, float hard) {
  if (t < 0. || t > .6) return 0.;
  float p = 46. * t + (150. + 60. * hard - 46.) * .032 * (1. - exp(-t / .032));
  return (sin(TAU * p) + .3 * hard * sin(TAU * p * 2.3) * exp(-t * 60.)) * env(t, .0015, 10.5 - 2. * hard) * smoothstep(.6, .4, t);
}
// band-passed noise burst (gaussian-windowed cosine kernel)
float bpn(int n, float fc, float q, uint seed) {
  float s = 0., w = TAU * fc / 48000.;
  int K = int(q * 48000. / fc);
  for (int i = -K; i <= K; i++) {
    float x = float(i) / float(K);
    s += nz(uint(n + i) + seed) * exp(-x * x * 4.) * cos(w * float(i));
  }
  return s * 2.5 / sqrt(float(K));
}
float hat(int n, float t, float d) {
  if (t < 0. || t > .5) return 0.;
  return (bpn(n, 8200., 1.4, 3U) + .6 * bpn(n, 11500., 1.6, 19U)) * .75 * env(t, .001, d);
}
float clap(int n, float t) {
  if (t < 0. || t > .5) return 0.;
  float e = exp(-t * 14.) + exp(-fract(t * 90.) * 6.) * step(t, .033) * .8;
  return bpn(n, 1500., 2.2, 77U) * e * smoothstep(0., .001, t);
}

// --- plucked / struck tone whose partials go from random to harmonic ------------------
float pluck(int k, float note, float dis, uint seed, float bright) {
  float t = float(k) / 48000., f0 = mtof(note), s = 0.;
  for (int h = 1; h <= 10; h++) {
    float fh = f0 * (float(h) + dis * (hf(seed + uint(h)) - .5) * 1.5 * float(h > 1));
    if (fh < 16000.) s += sn(k, fh) * exp(-t * (3. + (2.4 - 1.6 * bright) * float(h))) / float(h);
  }
  return s * smoothstep(0., .002, t) * smoothstep(.625, .5, t);
}
// bell: chord tone plus inharmonic plate partials
float bell(int k, float note) {
  float t = float(k) / 48000., f = mtof(note);
  return (sn(k, f) * exp(-t * 1.6) + .4 * sn(k, f * 2.) * exp(-t * 2.6) + .22 * sn(k, f * 2.76) * exp(-t * 5.) + .12 * sn(k, f * 5.4) * exp(-t * 9.)
          + .08 * sn(k, f * 8.93) * exp(-t * 14.)) * smoothstep(0., .003, t);
}

void main() {
  int n = int(gl_FragCoord.x) + int(gl_FragCoord.y) * 2048;
  float t = float(n) / 48000.;
  int bar = n / BARN, kb = n - bar * BARN;           // bar index, sample in bar
  int st = n / STEP, ks = n - st * STEP;             // global 16th index, sample in step
  int s16 = st & 15;
  float tb = float(kb) / 48000.;
  vec2 dry = vec2(0), rev = vec2(0), dly = vec2(0), hall = vec2(0);
  float kt = kickT(t), duck = 1. - .7 * exp(-kt * 7.) * step(12., t) * step(t, 56.);
  float gap = 1. - smoothstep(43.73, 43.76, t) + smoothstep(43.985, 44., t);   // breath before the drop

  // --- snow hiss + crackle (falls silent as the picture settles) ---
  float sa = snowAmt(t) * smoothstep(0., .8, t);
  vec2 hs = vec2(nz(uint(n)), nz(uint(n) + 9999U));
  vec2 hp = hs - vec2(nz(uint(n - 1)), nz(uint(n - 1) + 9999U));
  float roll = .65 + .35 * sin(TAU * t * .69);
  float crk = step(.9993, hf(uint(n / 3) * 3U + 17U)) * 1.6;         // crackle
  dry += (hs * .35 + hp * .3 + crk * hs) * sa * sa * .4 * roll;
  hall += hs * sa * .03;
  dry += vec2(sn(n, 55.) + .5 * sn(n, 110.2)) * .02 * sa * smoothstep(0., 2., t);   // mains hum under the static

  { // sync-loss glitches: a burst of crushed noise and a thump
    vec2 g = glitch(t);
    float gb = nz(uint(n / 6) * 7U + 5U) * g.x * g.x;
    dry += vec2(gb, -gb) * .22 + sin(TAU * 70. * (t - floor(t))) * g.x * g.x * .0;
    hall += gb * .12;
  }
  // --- drone on D: fades in while the cloud appears ---
  float dr = smoothstep(3., 9., t) * (1. - smoothstep(18., 21., t));
  float dn = (.5 * dsf(n, 36.71, .5) + .7 * dsf(n, 73.5, .5 + .2 * sin(t * .7)) + .5 * dsf(n, 110.1, .5) + .3 * dsf(n, 220.3, .45 + .2 * sin(t * .53))) * .06;
  dry += dn * dr * vec2(1., .95) * duck;
  rev += dn * dr * .5;

  // --- kick ---
  float kh = t < 12. ? 0. : (t < 20. ? .4 : 1.);
  float kv = t < 12. ? .45 : (t < 20. ? .7 : 1.);
  if (t >= 32. && t < 40.) { kv = .75; kh = .5; }
  float kk = kick(kt, kh) * kv * gap;
  dry += kk * .9;

  // --- hats / clap ---
  if (t >= 14. && t < 56.) {
    float hv = smoothstep(14., 20., t) * (t >= 32. && t < 40. ? .5 : 1.) * gap;
    float th8 = float((n + 12000) % 24000) / 48000.;                 // offbeat 8ths
    float h1 = hat(n, th8, t >= 44. ? 18. : 34.) * .24;
    float h2 = hat(n, float(ks) / 48000., 75.) * (t >= 20. ? .07 + .07 * hf(uint(st) * 7U) : 0.);   // 16ths
    vec2 hh = pan(h1, .38) + pan(h2, .5 + .3 * sin(float(st) * 2.1));
    dry += hh * hv; rev += hh * hv * .12;
  }
  if ((t >= 20. && t < 32.) || (t >= 44. && t < 56.)) { // clap on 2 and 4
    float tc = float((n + 24000) % 48000) / 48000.;
    float c = clap(n, tc) * .55;
    dry += c * vec2(.95, 1.); rev += c * .5;
  }
  if (t >= 44. && t < 56.) { // sheen of noise breathing with the kick
    vec2 air = hp * (1. - exp(-kt * 9.)) * .028;
    dry += air; rev += air * .3;
  }
  // --- glass voice over the galaxy: two long notes per bar (bars 10-15) ---
  if (t >= 20. && t < 33.5) {
    const float GN[12] = float[12](81., 77., 74., 77., 81., 84., 79., 76., 82., 79., 81., 85.);
    vec2 gl = vec2(0);
    for (int j = 0; j < 2; j++) {
      int i = (n - 20 * SR) / SR - j;
      if (i < 0 || i > 11) continue;
      int k = n - 20 * SR - i * SR;
      float tt = float(k) / 48000., f = mtof(GN[i]);
      float x = (sn(k, f) + .35 * sn(k, f * 2.) + .12 * sn(k, f * 3.) + .5 * sn(k + 311, f * 1.004)) * smoothstep(0., .03, tt) * exp(-tt * 1.8) * smoothstep(2., 1.7, tt);
      gl += pan(x, .3 + .4 * float(i & 1));
    }
    dry += gl * .05; rev += gl * .06; hall += gl * .09; dly += gl * .04;
  }
  // build: snare roll (8ths -> 16ths -> 32nds) and noise riser, bars 20-21
  if (t >= 40. && t < 44.) {
    float u = (t - 40.) / 4.;
    int div = t < 42. ? 12000 : (t < 43.5 ? 6000 : 3000);
    float tr = float(n % div) / 48000.;
    float sr = bpn(n, 1900. + 900. * u, 1.6, 31U) * env(tr, .001, 22.) * (.14 + .55 * u * u) * gap;
    dry += sr; rev += sr * .4;
    float ri = bpn(n, 500. * exp2(u * 4.2), 1.5, 5U) * u * u * .2 * gap;
    dry += ri * vec2(1., .9); hall += ri * .5;
  }

  for (int j = 0; j < 2; j++) { // one-bar noise swells into the galaxy and the plate
    float u = (t - (j == 0 ? 18. : 30.)) * .5;
    if (u < 0. || u > 1.) continue;
    float sw = bpn(n, 700. * exp2(u * 3.), 1.2, 11U) * u * u * u * .13;
    dry += sw * vec2(.9, 1.); hall += sw * .6;
  }

  // --- bass ---
  if (t >= 12. && t < 56.) {
    vec4 cn = chordNotes(chordId(bar));
    int k8 = n % 12000, i8 = (n / 12000) & 7;                 // 8th notes
    float t8 = float(k8) / 48000.;
    float bn = cn.w + ((t >= 44. && (i8 & 1) == 1) ? 12. : 0.);  // octave bounce in the last section
    float f = mtof(bn);
    float b = (dsf(k8, f, .7 * exp(-t8 * 6.) + .3) * .7 + sn(k8, f) * .45) * env(t8, .004, 2.5) * smoothstep(.25, .2, t8);
    if (t >= 32. && t < 40.) b = (dsf(kb, mtof(cn.w), .45) * .6 + sn(kb, mtof(cn.w)) * .45) * env(tb, .01, .5) * smoothstep(2., 1.9, tb);
    dry += b * .3 * duck * smoothstep(12., 13., t) * gap;
  }

  // --- pad: three chord notes, voices spread over two octaves and across the stereo field ---
  if (t >= 6. && t < 58.) {
    vec2 pd = vec2(0);
    for (int j = 0; j < 2; j++) {                                 // this bar and the tail of the previous one
      int b = bar - j, k = kb + j * BARN;
      if (b < 3) continue;
      float tt = float(k) / 48000.;
      float e = j == 0 ? smoothstep(0., .3, tt) : exp(-(tt - 2.) * 4.5);
      if (j == 0 && b == 28) e *= exp(-max(tt - .1, 0.) * 1.9);    // final chord rings out
      vec4 cn = chordNotes(chordId(b));
      float br = .56 + .2 * smoothstep(20., 46., t) + .06 * sin(t * .9);
      for (int v = 0; v < 12; v++) {
        int w = v % 3;
        float note = (v < 3 ? cn.x : v < 6 ? cn.y : v < 9 ? cn.z : cn.x + 12.) + (w == 1 ? 0. : 12.);
        float det = (float(w) - 1.) * .13 + (hf(uint(v) * 13U) - .5) * .04;
        float x = dsf(k + v * 977, mtof(note + det), br) * (w == 1 ? .6 : 1.);
        pd += pan(x, w == 0 ? .08 : w == 1 ? .5 : .92) * e;
      }
    }
    float pv = smoothstep(6., 12., t) * (t >= 44. && t < 56. ? 1.3 : 1.) * .034 * gap;
    dry += pd * pv * duck; rev += pd * pv * .8;
  }

  // --- plucks: random onsets / pitches / partials that settle into a 16th-note arpeggio ---
  if (t >= .8 && t < 57.) {
    const int AP[16] = int[16](0, 1, 2, 3, 4, 3, 2, 1, 0, 2, 3, 4, 5, 4, 3, 2);
    const int PENT[5] = int[5](0, 3, 5, 7, 10);
    vec2 pl = vec2(0);
    for (int j = 0; j < 5; j++) {
      int s = st - j;
      uint sd = uint(s) * 2654435761U;
      float ts = float(s) * .125;
      float d = disorder(ts);
      // onset: jittered inside its slot while disordered; fewer notes early on
      int on = s * STEP + int((hf(sd) - .5) * (1. - smoothstep(4., 14., ts)) * 5200.);
      int k = n - on;
      if (k < 0 || k > 30000 || ts >= 56.) continue;
      float prob = mix(1., .16, d * d) * smoothstep(.8, 1.6, ts);
      if (hf(sd + 1U) > prob) continue;
      vec4 cn = chordNotes(chordId(max(s / 16, 4)));
      int a = AP[s & 15];
      float note = (a % 3 == 0 ? cn.x : a % 3 == 1 ? cn.y : cn.z) + 12. + 12. * float(a / 3);
      // pitch: free and microtonal over the static, then random notes of the scale, then the pattern
      if (ts < 8.) note = 64. + (hf(sd + 2U) - .5) * 30.;
      else if (hf(sd + 4U) > smoothstep(10., 16.5, ts)) note = 62. + float(PENT[int(hf(sd + 2U) * 5.)]) + 12. * floor(hf(sd + 5U) * 2.);
      float v = .55 + .45 * hf(sd + 3U) * d + ((s & 3) == 0 ? .25 : 0.);
      float sec = ts < 20. ? 1. : (ts < 32. ? .8 : (ts < 44. ? .6 : .5));
      float inh = 1. - smoothstep(4., 11., ts);
      pl += pan(pluck(k, note, inh, sd, .5 + .45 * smoothstep(16., 44., ts)) * v * sec, .5 + .42 * sin(float(s) * 2.4));
    }
    pl *= gap * smoothstep(56.05, 55.97, t);
    dry += pl * .2; rev += pl * .14; dly += pl * .17 * smoothstep(55.6, 55.1, t);
  }

  // --- resonance: bell chords struck on beats 1 and 3 of bars 16-19 (they drive the Chladni modes) ---
  if (t >= 32. && t < 42.5) {
    vec2 bl = vec2(0);
    for (int j = 0; j < 3; j++) {
      int hi = (n - 32 * SR) / SR - j;                           // strike index (one per second)
      if (hi < 0 || hi > 7) continue;
      int k = n - 32 * SR - hi * SR;
      vec4 cn = chordNotes(chordId(16 + hi / 2));
      float up = (hi & 1) == 1 ? 12. : 0.;
      bl += pan(bell(k, cn.x + 12. + up), .2) + pan(bell(k + 211, cn.y + 12. + up), .8) + pan(bell(k + 97, cn.z + 12. + up), .5) + pan(bell(k, cn.x + 24.) * .5, .65);
    }
    dry += bl * .12; rev += bl * .1; hall += bl * .12;
  }

  // --- lead motif over the word (bars 22-27), dotted rhythm 3+3+2 ---
  if (t >= 44. && t < 58.) {
    const int LS[6] = int[6](0, 3, 6, 8, 11, 14);
    const float LN[36] = float[36](78., 76., 74., 69., 74., 76.,  76., 73., 69., 76., 73., 76.,  78., 74., 71., 78., 74., 78.,
                                   79., 74., 71., 79., 81., 83.,  81., 78., 74., 81., 78., 81.,  81., 76., 73., 76., 81., 85.);
    vec2 ld = vec2(0);
    int b = bar - 22, i = 0;
    for (int q = 0; q < 6; q++) if (s16 >= LS[q]) i = q;
    for (int j = 0; j < 4; j++) {                                 // the last few notes may still ring
      int idx = min(b, 6) * 6 + (b >= 6 ? 0 : i) - j;
      if (idx < 0 || idx > 36) continue;
      int nb = idx / 6, ni = idx - nb * 6;
      int on = (22 + nb) * BARN + (idx == 36 ? 0 : LS[ni] * STEP);
      float note = idx == 36 ? 86. : LN[idx];
      int k = n - on;
      if (k < 0) continue;
      float tt = float(k) / 48000.;
      float x = (dsf(k, mtof(note), .72 * exp(-tt * 4.) + .3) + dsf(k + 400, mtof(note + .1), .65 * exp(-tt * 4.) + .3)
                 + .5 * dsf(k + 90, mtof(note - 12.), .5)) * env(tt, .004, idx == 36 ? 1.5 : 4.);
      if (idx < 36) x *= smoothstep(56.05, 55.97, t);             // nothing but the tonic may ring into the final chord
      ld += pan(x, .4 + .2 * float(idx & 1));
    }
    dry += ld * .11 * duck; rev += ld * .1; dly += ld * .11 * smoothstep(55.6, 55.1, t);
    // chord stabs on the same dotted rhythm give the drop its drive
    if (b >= 0 && b < 6) {
      int k = n - ((22 + b) * BARN + LS[i] * STEP);
      float tt = float(k) / 48000.;
      vec4 cn = chordNotes(chordId(bar));
      vec2 sb = vec2(0);
      for (int v = 0; v < 6; v++) {
        float note = (v < 2 ? cn.x : v < 4 ? cn.y : cn.z) + 12. + ((v & 1) == 0 ? -.08 : .08);
        sb += pan(dsf(k + v * 331, mtof(note), .75 * exp(-tt * 7.) + .2), (v & 1) == 0 ? .1 : .9);
      }
      sb *= env(tt, .003, 7.) * smoothstep(.33, .28, tt);
      dry += sb * .045 * duck; rev += sb * .05;
    }
  }

  // --- impacts at the big scene changes, and the switch-off at the end ---
  for (int j = 0; j < 4; j++) {
    float ti = j == 0 ? 20. : j == 1 ? 32. : j == 2 ? 44. : 56.;
    float u = t - ti;
    if (u < 0. || u > 3.) continue;
    float boom = sin(TAU * (34. * u + 55. * .08 * (1. - exp(-u / .08)))) * env(u, .002, 2.6);
    vec2 crash = (hs - vec2(nz(uint(n - 1)), nz(uint(n - 1) + 9999U)) * .6) * env(u, .002, 2.4 + float(j == 1) * 3.);
    dry += boom * .4 + crash * .1; hall += crash * .12 + boom * .08;
  }
  {
    float u = t - 56.;
    if (u > 0. && u < .6) { // CRT switch-off chirp
      float c = sin(TAU * 2600. * .09 * (1. - exp(-u / .09))) * env(u, .002, 9.) * .12;
      dry += c; rev += c * .5;
    }
  }
  float fade = smoothstep(0., .3, t) * (1. - smoothstep(57.3, 57.85, t));
  o0 = vec4(dry, rev) * fade;
  o1 = vec4(dly, hall) * fade;
}
