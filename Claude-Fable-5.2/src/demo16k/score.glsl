// ---- tempo / score / schedule shared by simulation, image and sound ----
const float BPM = 96., BEAT = .625, BAR = 2.5, S16 = .15625;
const float SPS = 3600.;            // simulation steps per second of demo time
const vec2 SIM = vec2(960, 540);    // simulation grid (60 texels = 1 world unit, world = 16 x 9, periodic)

// kick drum on 16th slot i ?
bool kickOn(int i) {
  int b = i >> 4, s = i & 15;
  if (b < 4 || b > 20) return false;
  if (b < 6) return s == 0 || s == 8 && b == 5;
  if (b < 10) return (s & 7) == 0;
  if (b < 15) return (s & 7) == 0 || s == 14 && (b & 1) == 1 || s == 6 && b > 11;
  if (b == 15) return s == 0 || s == 6;
  if (b < 20) return (s & 3) == 0;
  return s == 0;
}
// slot of the most recent kick at or before slot i (-999 if none)
int kickSlot(int i) {
  for (int j = 0; j < 32; j++) if (i >= j && kickOn(i - j)) return i - j;
  return -999;
}
float kickAge(float t) { return t - float(kickSlot(int(floor(t / S16)))) * S16; }

// harmony: chord (4 MIDI notes) of half-bar h, bass root of bar b
const ivec4 CH[11] = ivec4[](ivec4(50, 57, 62, 65), ivec4(50, 53, 60, 64), ivec4(46, 53, 57, 62), ivec4(53, 57, 60, 64), ivec4(48, 55, 62, 64), ivec4(55, 58, 62, 65),
                             ivec4(57, 62, 64, 67), ivec4(50, 57, 64, 66), ivec4(55, 59, 62, 66), ivec4(57, 62, 64, 69), ivec4(50, 57, 62, 66));
const int CB[24] = int[](0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 7, 8, 9, 10, 10, 10, 10);
const int BS[11] = int[](38, 38, 34, 41, 36, 43, 45, 38, 43, 45, 38);
ivec4 chord(int h) {
  int b = clamp(h >> 1, 0, 23);
  ivec4 c = CH[CB[b]];
  if ((b == 15 || b == 19) && (h & 1) == 1) c.y--;   // sus4 resolves
  return c;
}

// "droplet" note of 16th slot i: (start time or -1, pitch [MIDI], world x, world y)
// timing starts random and locks to the grid, pitch starts detuned and locks to the scale, then to the chords
vec4 drop(int i) {
  int b = i >> 4, s = i & 15;
  float r = hf(uint(i) * 7U + 3U), r2 = hf(uint(i) * 7U + 2U), bt = float(i) / 16., p;
  bool on = b < 6 ? r < mix(.07, .3, bt / 6.)
          : b < 10 ? (s & 1) == 0 && r < .8 || r < .1
          : b < 20 ? ((0x6D6D >> s) & 1) == 1
          : b < 22 ? (s & 3) == 0 : b == 22 && s == 0;
  ivec4 c = chord(i >> 3);
  if (b < 6) {
    int k = int(r2 * 10.);
    p = 62. + float(int[](0, 3, 5, 7, 10)[k % 5] + 12 * (k / 5)) + (hf(uint(i) * 7U + 1U) - .5) * 7. * clamp(1. - (bt - 1.) / 4.5, 0., 1.);
  } else if (b < 10) p = float(c[int(r2 * 4.)] + 12 + 12 * int(r * 1.3));
  else if (b < 20) {
    int e = bitCount(0x6D6D & ((1 << s) - 1)) % 6;
    p = float(c[e < 4 ? e : 6 - e] + (b < 16 ? 12 : 24) - 12 * (b & 1) * int(b > 15 && s < 8));
  } else p = float(int[](78, 74, 69, 66, 62, 69, 74, 78, 74)[((i - 320) >> 2) % 9]);
  float jit = (hf(uint(i) * 7U + 4U) - .5) * .9 * clamp(1. - (bt - 3.) / 5., 0., 1.);
  return vec4(on ? (float(i) + jit) * S16 : -1., p, 16. * hf(uint(i) * 7U + 5U), 9. * hf(uint(i) * 7U + 6U));
}

// lead line of the resolution (from bar 16): (beat, length in beats, MIDI note)
vec3 lead(int j) {
  return vec3[](vec3(0, 2, 78), vec3(2, 1, 76), vec3(3, 1, 74), vec3(4, 3, 81), vec3(7, 1, 78), vec3(8, 2, 79),
                vec3(10, 1, 83), vec3(11, 1, 81), vec3(12, 3, 76), vec3(15, 1, 73), vec3(16, 6, 74), vec3(16, 6, 81))[j];
}

// ---- the word: distance to the stroke centre lines of "MORPH" (world units, centred on the 16 x 9 field) ----
float seg(vec2 p, vec2 a, vec2 b) { p -= a; b -= a; return length(p - b * clamp(dot(p, b) / dot(b, b), 0., 1.)); }
float bowl(vec2 p) { return max(abs(seg(p, vec2(-1, 2.25), vec2(1.2, 2.25)) - .75), -p.x); }
float textD(vec2 p) {
  p -= vec2(1.55, 3.);
  float d = min(min(seg(p, vec2(0), vec2(0, 3)), seg(p, vec2(0, 3), vec2(1.2, .9))), min(seg(p, vec2(1.2, .9), vec2(2.4, 3)), seg(p, vec2(2.4, 3), vec2(2.4, 0))));   // M
  d = min(d, abs(seg(p, vec2(4.05, 1.1), vec2(4.05, 1.9)) - 1.1));                                                                                                   // O
  p.x -= 5.7;
  d = min(d, min(min(seg(p, vec2(0), vec2(0, 3)), bowl(p)), seg(p, vec2(.9, 1.5), vec2(1.95, 0))));                                                                  // R
  p.x -= 2.55;
  d = min(d, min(seg(p, vec2(0), vec2(0, 3)), bowl(p)));                                                                                                             // P
  p.x -= 2.55;
  return min(d, min(min(seg(p, vec2(0), vec2(0, 3)), seg(p, vec2(2.1, 0), vec2(2.1, 3))), seg(p, vec2(0, 1.5), vec2(2.1, 1.5))));                                    // H
}
