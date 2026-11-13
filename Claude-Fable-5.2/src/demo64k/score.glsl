// ---- timeline shared by picture and sound (120 BPM, 29 bars = 58 s) -----------
const float BEAT = .5, BAR = 2.;
// scene starts: cloud/web, galaxy, phyllotaxis, chladni plate, lattice, MORPH, outro, end
const float T_FLOW = 8., T_GRAV = 20., T_SPH = 26., T_RES = 32., T_LAT = 40., T_MORPH = 44., T_OUT = 56., T_END = 58.;

// seconds since the most recent kick drum (9 = none yet)
float kickT(float t) {
  if (t < 8.) return 9.;
  if (t < 12.) return mod(t, 1.);
  if (t < 32.) return mod(t, .5);
  if (t < 40.) return mod(t, 1.);
  if (t < 43.) return mod(t, .5);
  if (t < 43.75) return mod(t, .25);
  if (t < 44.) return t - 43.5;
  if (t < 56.) return mod(t, .5);
  return t - 56.;
}
float kickEnv(float t) { return exp(-5. * kickT(t)); }

// seconds since the most recent clap (beats 2 and 4 of the galaxy and of the word section)
float clapT(float t) { return ((t >= 20.5 && t < 32.) || (t >= 44.5 && t < 56.)) ? mod(t - .5, 1.) : 9.; }
// 0..1 over the build before the drop
float riser(float t) { return sat((t - 40.) / 3.75) * step(t, 43.75); }

// seconds since the last hit of the snare roll in the build (8ths, 16ths, 32nds), 9 outside
float rollT(float t) { return (t < 40. || t >= 43.75) ? 9. : mod(t, t < 42. ? .25 : (t < 43.5 ? .125 : .0625)); }

// Chladni modes (n, m, sign) for the 8 half-bar chord hits of the plate scene
vec3 chMode(int i) {
  const vec3 M[8] = vec3[8](vec3(1, 3, 1), vec3(2, 4, 1), vec3(3, 5, 1), vec3(2, 6, -1), vec3(4, 6, 1), vec3(5, 7, 1), vec3(5, 9, 1), vec3(4, 8, 1));
  return M[clamp(i, 0, 7)];
}

// picture glitches while the signal is still noise: x = envelope of the most recent one, y = its index
vec2 glitch(float t) {
  const float G[6] = float[6](1.62, 2.87, 3.71, 4.93, 5.38, 6.6);
  vec2 g = vec2(0);
  for (int i = 0; i < 6; i++) if (t >= G[i]) g = vec2(exp(-(t - G[i]) * 9.) * (1. - float(i) * .12), float(i));
  return g;
}
// "entropy" read-out (1 = pure noise, 0 = order), falling scene by scene
float entropy(float t) {
  const vec2 K[9] = vec2[9](vec2(0., 1.), vec2(1.2, 1.), vec2(8., .86), vec2(20., .61), vec2(26., .44), vec2(32., .31), vec2(40., .17), vec2(44., .08), vec2(46., 0.));
  float e = 0.;
  for (int i = 0; i < 8; i++) if (t >= K[i].x) e = mix(K[i].y, K[i + 1].y, sat((t - K[i].x) / (K[i + 1].x - K[i].x)));
  return e;
}
// amount of "snow" (0..1): drives the static on screen and the hiss in the sound
float snowAmt(float t) { return 1. - smoothstep(1.2, 7.4, t); }
// disorder of the music (1 = random pitches / timing / inharmonic partials, 0 = in tune and on the grid)
float disorder(float t) { return 1. - smoothstep(3., 17., t); }

// ---- harmony: one chord per bar ------------------------------------------------
// chord ids: 0 Dm, 1 Bb, 2 F, 3 C, 4 Gm, 5 A, 6 D, 7 Bm, 8 G
int chordId(int bar) {
  const int C[29] = int[29](0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 5, 7, 8, 6, 5, 6);
  return C[clamp(bar, 0, 28)];
}
// three chord notes (MIDI, close voicing around D3) and the bass note in w
vec4 chordNotes(int id) {
  const vec4 N[9] = vec4[9](vec4(50, 53, 57, 38), vec4(50, 53, 58, 34), vec4(48, 53, 57, 41), vec4(48, 52, 55, 36), vec4(50, 55, 58, 31),
                            vec4(49, 52, 57, 33), vec4(50, 54, 57, 38), vec4(50, 54, 59, 35), vec4(50, 55, 59, 31));
  return N[id];
}
