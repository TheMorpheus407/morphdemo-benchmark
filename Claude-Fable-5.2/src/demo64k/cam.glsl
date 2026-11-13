// ---- camera -----------------------------------------------------------------
// per-scene exposure of the grains
float expo(float t) {
  float e = mix(.16, .25, smoothstep(5., 14., t));
  e = mix(e, .06, smoothstep(T_GRAV - 1.2, T_GRAV + .2, t));
  e = mix(e, .13, smoothstep(T_GRAV + .2, T_GRAV + 2., t));
  e = mix(e, .10, smoothstep(T_SPH - .5, T_SPH + 1.5, t));
  e = mix(e, .075, smoothstep(T_RES - .8, T_RES + .2, t));
  e = mix(e, .03, smoothstep(T_LAT, T_LAT + 1.2, t));
  e = mix(e, .2, smoothstep(T_MORPH - 1.3, T_MORPH - .2, t));
  e = mix(e, .2, smoothstep(T_MORPH, T_MORPH + 1.5, t));
  return e;
}
// fog density (per unit distance)
float fogD(float t) { return mix(.1, .55, smoothstep(T_LAT + .3, T_LAT + 1.2, t) - smoothstep(T_MORPH - 1.1, T_MORPH - .2, t)); }
// one camera key: position, target, cp = (focal length, focus distance (0 = on target), aperture)
void camKey(int k, float t, out vec3 ro, out vec3 ta, out vec3 cp) {
  ta = vec3(0);
  if (k == 0) { // cloud / web: slow orbit, drifting in
    float a = t * .17 + .3, d = mix(2.6, 5.9, smoothstep(3., 13.5, t)) - max(t - 13.5, 0.) * .07;
    ro = vec3(d * sin(a), 1.2 * sin(t * .07 + 1.), d * cos(a)); cp = vec3(1.7, 0., .024);
  } else if (k == 1) { // galaxy: oblique, rising
    float a = t * .2, el = mix(.38, 1.3, smoothstep(T_GRAV + 1.5, T_SPH + .5, t));
    ro = 6. * vec3(sin(a) * cos(el), sin(el), cos(a) * cos(el)); cp = vec3(1.9, 0., .014);
  } else if (k == 2) { // phyllotaxis: top view coming down to the side as it wraps into a sphere
    float el = mix(1.45, .25, smoothstep(T_SPH + 1.5, T_RES - 1., t)), a = t * .1;
    ro = 6.3 * vec3(sin(a) * cos(el), sin(el), cos(a) * cos(el)); ta = vec3(0, .2 * smoothstep(28., 30.5, t), 0); cp = vec3(2., 0., .012);
  } else if (k == 3) { // plate: orbiting three-quarter view
    float a = (t - T_RES) * .23 + .55, el = .74 + .1 * sin(t * .3);
    ro = 6.5 * vec3(sin(a) * cos(el), sin(el), cos(a) * cos(el)); ta = vec3(0, -.5, 0); cp = vec3(2.1, 0., .007);
  } else if (k == 4) { // lattice: glide between the rows, then pull back out through the front
    float u = t - T_LAT, o = smoothstep(2.7, 4., u);
    ro = mix(vec3(-2.6 + u * .55, .22, -.36), vec3(0., .12, 5.5), o);
    ta = mix(vec3(ro.x + 2., .05, -.2), vec3(0), o); cp = vec3(mix(1.25, 2.1, o), mix(.9, 0., o), mix(.02, .012, o));
    if (o > 0. && o < 1.) cp.y = mix(.9, 5.5, o);
  } else { // word: frontal, slow push-in with a little parallax
    float u = t - T_MORPH;
    float sw = smoothstep(0., 12., u);
    ro = vec3(-1.1 + 2.3 * sw, -.05 + .5 * sw, 5.4 - .4 * sin(sw * PI) + .25 * sw); ta = vec3(.3 * (sw - .35), -.2, 0); cp = vec3(2.1 * (1. + .014 * exp(-kickT(t) * 9.)), 0., .012);
  }
  if (cp.y == 0.) cp.y = distance(ro, ta);
}
void camBlend(int k, float t, float w, inout vec3 ro, inout vec3 ta, inout vec3 cp) {
  vec3 a, b, c;
  camKey(k, t, a, b, c);
  w = w * w * (3. - 2. * w);
  ro = mix(ro, a, w); ta = mix(ta, b, w); cp = mix(cp, c, w);
}
void camera(float t, out vec3 ro, out vec3 cu, out vec3 cv, out vec3 cw, out vec3 cp) {
  vec3 ta;
  camKey(0, t, ro, ta, cp);
  if (t > T_GRAV - 1.) camBlend(1, t, sat((t - T_GRAV + 1.) / 2.5), ro, ta, cp);
  if (t > T_SPH - 1.) camBlend(2, t, sat((t - T_SPH + 1.) / 2.5), ro, ta, cp);
  if (t > T_RES - 1.2) camBlend(3, t, sat((t - T_RES + 1.2) / 2.4), ro, ta, cp);
  if (t > T_LAT - .3) camBlend(4, t, sat((t - T_LAT + .3) / 1.6), ro, ta, cp);
  if (t > T_MORPH - .3) camBlend(5, t, sat((t - T_MORPH + .3) / 1.2), ro, ta, cp);
  cw = normalize(ta - ro);
  cu = normalize(cross(cw, vec3(0, 1, 0)));
  cv = cross(cu, cw);
  // slow roll while gliding through the crystal, level again for the reveal
  float rl = .55 * sq(sin(PI * sat((t - T_LAT - .7) / 2.6)));
  vec3 u = cu * cos(rl) + cv * sin(rl);
  cv = cv * cos(rl) - cu * sin(rl);
  cu = u;
}
