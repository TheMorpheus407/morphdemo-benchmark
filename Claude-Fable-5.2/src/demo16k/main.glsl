// Relief rendering of the reaction-diffusion field (HDR; alpha = view distance for depth of field).
// U[4] = (target x, target y, distance, pitch)  U[5] = (yaw, tan(fov/2), focus distance, aperture)
// U[6] = (fade, bloom, grain, boiling noise)     U[7] = (glow, accent, light turn, -)
layout(binding=0) uniform sampler2D S;
layout(binding=1) uniform sampler2D P;      // the field one simulation step earlier
out vec4 o;
float hs;                                  // relief height (world units; 60 sim texels = 1 unit)
// bicubic B-spline lookup of the field (4 bilinear taps); p in world units
vec4 F(vec2 p) {
  p = p * 60. - .5;
  vec2 f = fract(p), b = p - f, f2 = f * f, f3 = f2 * f;
  vec2 w0 = (1. - f) * (1. - f) * (1. - f) / 6., w1 = (3. * f3 - 6. * f2 + 4.) / 6., w3 = f3 / 6., w2 = 1. - w0 - w1 - w3;
  vec2 g0 = w0 + w1, g1 = w2 + w3, h0 = (b + w1 / g0 - .5) / SIM, h1 = (b + w3 / g1 + 1.5) / SIM;
  return g0.y * (g0.x * textureLod(S, vec2(h0.x, h0.y), 0.) + g1.x * textureLod(S, vec2(h1.x, h0.y), 0.))
       + g1.y * (g0.x * textureLod(S, vec2(h0.x, h1.y), 0.) + g1.x * textureLod(S, vec2(h1.x, h1.y), 0.));
}
float H(vec2 p) { return hs * smoothstep(0., .3, F(p).y); }
// value noise
float vn(vec2 p) {
  vec2 i = floor(p), f = p - i;
  f = f * f * (3. - 2. * f);
  uint n = uint(int(i.x) + int(i.y) * 7919 + 100000);
  return mix(mix(hf(n), hf(n + 1U), f.x), mix(hf(n + 7919U), hf(n + 7920U), f.x), f.y);
}
// environment seen in reflections: dark sky, cream soft box around the key light, dim magenta strip opposite
vec3 env(vec3 r, vec3 L) {
  vec3 d = r - L; d.z *= 2.2;
  vec3 e = r + vec3(L.xy, -.25 * L.z); e.z *= 2.2;
  return mix(vec3(.004, .012, .022), vec3(.01, .07, .09), smoothstep(-.1, .9, r.z)) + vec3(.03, .2, .24) * exp(-r.z * r.z * 9.)   // dark dome, glowing horizon
       + vec3(1, .9, .72) * 1.2 * smoothstep(.5, .2, length(d)) + vec3(.9, .1, .4) * .7 * smoothstep(.8, .3, length(e));
}
void main() {
  vec2 q = (gl_FragCoord.xy * 2. - RES) / RES.y;
  float pi = U[4].w, ya = U[5].x, ka = kickAge(TIME), ke = exp(-ka * 5.);
  hs = .2 * (1. + .13 * ke);
  vec3 ta = vec3(U[4].xy, 0), fw = -vec3(cos(pi) * cos(ya), cos(pi) * sin(ya), sin(pi));
  vec3 ro = ta - fw * U[4].z, rt = normalize(cross(fw, vec3(-cos(ya) * .001, -sin(ya) * .001, 1))), up = cross(rt, fw);
  vec3 rd = normalize(fw + U[5].y * (1. - .018 * ke) * (q.x * rt + q.y * up));   // the lens punches in a little on every kick
  float jit = hf(uint(gl_FragCoord.x) * 1973U + uint(gl_FragCoord.y) * 9277U + uint(U[0].w) * 26699U);
  float la = ya + 2.05 + U[7].z;
  vec3 L = normalize(vec3(cos(la), sin(la), .6));
  vec3 fog = vec3(.003, .008, .016) + vec3(.0, .03, .04) * pow(max(dot(rd.xy, L.xy), 0.), 3.) * smoothstep(-.4, .1, rd.z);
  vec3 col = fog;
  float dist = 1e3;
  if (rd.z < -.01) {
    float t0 = (hs - ro.z) / rd.z, t1 = -ro.z / rd.z, dt = (t1 - t0) / 64., t = t0 + dt * jit, tp = t0;
    for (int i = 0; i < 64; i++) { vec3 p = ro + rd * t; if (H(p.xy) > p.z) break; tp = t; t += dt; }
    for (int i = 0; i < 6; i++) { float tm = .5 * (tp + t); vec3 p = ro + rd * tm; if (H(p.xy) > p.z) t = tm; else tp = tm; }
    vec3 p = ro + rd * t;
    dist = t;
    vec4 c = F(p.xy);
    float e = 1. / 60., v = smoothstep(0., .42, c.y);
    vec3 n = vec3(H(p.xy - vec2(e, 0)) - H(p.xy + vec2(e, 0)), H(p.xy - vec2(0, e)) - H(p.xy + vec2(0, e)), 2. * e);
    // boiling micro relief: the "noise" the culture starts from
    vec2 np = p.xy * 70. + floor(TIME * 18.) * 7.3, nq = vec2(np.x * .8 - np.y * .6, np.x * .6 + np.y * .8) * 2.3;
    n.xy += e * (vec2(vn(np) + .6 * vn(nq), vn(np + 31.7) + .6 * vn(nq + 17.3)) - .8) * U[6].w;
    // droplet notes: ripples running over the wet surface from where they land
    float fl = 0.;
    for (int i = 0; i < 9; i++) {
      vec4 d = drop(int(floor(TIME / S16)) - i);
      float a = TIME - d.x;
      if (d.x >= 0. && a > 0.) {
        vec2 w = p.xy - d.zw; w -= vec2(16, 9) * floor(w / vec2(16, 9) + .5);
        float r = length(w), x = r - a * 1.3;
        n.xy += e * w / (r + .01) * sin(x * 30.) * exp(-x * x * 12. - a * 2.8) * 1.6;
        fl += exp(-r * r * 40. - a * 7.) + .35 * exp(-x * x * 14. - a * 1.7);
      }
    }
    n = normalize(n);
    // key light shadow (short march over the height field)
    float sh = 1.;
    for (int i = 1; i < 14; i++) { vec3 s = p + L * (float(i) + jit) * .016; sh = min(sh, 1. + 7. * (s.z - H(s.xy)) / (float(i) * .016 + .016)); }
    sh = clamp(sh, 0., 1.);
    float bl = smoothstep(0., .42, textureLod(S, p.xy / vec2(16, 9), 3.5).y);
    float ao = clamp(1. - 1.8 * (bl - v), .2, 1.) * mix(.55, 1., v);
    float ndv = clamp(dot(n, -rd), 0., 1.), fr = .03 + .45 * pow(1. - ndv, 5.);
    vec3 hv = normalize(L - rd);
    float nh = clamp(dot(n, hv), 0., 1.), nl = clamp(dot(n, L) * .75 + .25, 0., 1.);
    // every kick sends a ring through the tissue (same ring that starves the simulation)
    float rr = length(p.xy - U[4].xy) - ka * 7., ring = exp(-rr * rr * 70. - ka * 2.6) * smoothstep(0., .18, ka);
    float wave = .55 + .45 * sin(p.x * .5 - TIME * .6 + 2. * sin(p.y * .4 + TIME * .23));
    // every note of the lead line sends a front of light across the lattice
    float le = 0.;
    for (int j = 0; j < 11; j++) {
      float a = TIME - 40. - lead(j).x * BEAT, x = p.x + .35 * p.y - a * 11. + 3.;
      if (a > 0.) le += exp(-x * x * .8 - a * 1.2);
    }
    float b5 = smoothstep(0., .3, textureLod(S, p.xy / vec2(16, 9), 4.5).y);
    // metabolism: where the culture is feeding (substrate dropping) it lights up
    float gr = max((textureLod(P, p.xy / vec2(16, 9), 0.).x - textureLod(S, p.xy / vec2(16, 9), 0.).x) / max(U[2].z, .05), 0.) * 1200. * (1. - smoothstep(0., .004, U[1].z));   // (not while the whole pattern is drifting)
    vec3 teal = mix(vec3(0, .55, .42), vec3(.0, .4, .62), .5 + .5 * sin(p.x * .7 + p.y * .5 + TIME * .2));
    vec3 body = mix(vec3(.003, .007, .014), teal * .3 * (.8 + .4 * vn(p.xy * 3.)), v);             // pigment clouds
    col = body * (.1 * ao + nl * sh * vec3(1, .92, .78) * .8)                                      // diffuse
        + teal * (v * v * (.22 + .5 * wave + .5 * ke) * ao + .07 * b5 * (1. - v)) * U[7].x          // inner glow (drifting, breathing with the kick) + spill on the ground
        + mix(teal, vec3(1), .45) * v * pow(1. - ndv, 3.) * .6 * U[7].x                            // translucent rim
        + vec3(1., .2, .42) * U[7].y * ring * (.04 + v) * 1.3                                        // magenta kick ring
        + vec3(1., .2, .3) * U[7].y * min(gr, 2.) * (.12 + v) * .8                                  // coral growth fronts
        + vec3(.75, 1, .9) * fl * (.05 + v) * 1.4                                                  // droplet light
        + vec3(1, .9, .7) * le * v * .9                                                             // lead line light
        + vec3(1, .93, .8) * (7. * pow(nh, 500.) + .08 * pow(nh, 30.)) * sh * min(.02 + .98 * v + U[6].w, 1.)  // wet highlights (glitter on the boiling ground)
        + env(reflect(rd, n), L) * fr * mix(.5, 1., ao) * (.04 + .96 * v)
          * mix(vec3(1), .5 + .5 * cos(TAU * (ndv * 1.3 + .15 * p.x) + vec3(0, 2.1, 4.2)) + .5, .35);            // pearlescent film
    float fa = max(t / U[4].z - 1., 0.);
    col = mix(fog, col, exp(-fa * fa * 1.6));
  }
  if (any(isnan(col))) col = vec3(0);
  o = vec4(col, dist);
}
