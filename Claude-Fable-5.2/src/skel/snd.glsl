layout(location=0) out vec4 o0;
vec2 synth(float t) {
  float beat = t * BPM / 60., ph = fract(beat), tb = ph * 60. / BPM;
  float kick = sin(6.2831853 * (50. * tb + 12. * (1. - exp(-tb * 30.)))) * exp(-tb * 7.);
  float note = 220. * exp2(floor(hash(floor(beat * 2.)) * 5.) * 2. / 12.);
  float tn = fract(beat * 2.) * 30. / BPM;
  float pl = sin(6.2831853 * note * tn + 2. * sin(6.2831853 * note * 2. * tn) * exp(-tn * 8.)) * exp(-tn * 6.);
  return vec2(kick * .8, pl * .25);
}
void main() {
  int n = int(gl_FragCoord.x) + int(gl_FragCoord.y) * 2048;
  vec2 s = synth(float(n) / 48000.);
  o0 = vec4(s.x + s.y * .3, s.x + s.y * .3, s.y, s.y);   // rg = dry, ba = reverb send
}
