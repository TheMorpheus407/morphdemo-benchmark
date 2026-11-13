out vec4 outColor;
void main() {
  vec2 p = (gl_FragCoord.xy * 2. - RES) / RES.y;
  float beat = TIME * BPM / 60.;
  float kick = exp(-5. * fract(beat));
  float ring = smoothstep(.03, 0., abs(length(p) - .5 - .15 * kick));
  vec3 col = ring * (.5 + .5 * cos(atan(p.y, p.x) * 2. + TIME + vec3(0, 2, 4)));
  col += vec3(.9, .4, .1) * kick * smoothstep(.25, .0, length(p));
  // orientation marker: bright block in the TOP-LEFT corner
  col += vec3(0, .6, .2) * step(p.x, -1.5) * step(.8, p.y);
  outColor = vec4(col, 1);
}
