// ---- particles: position = f(id, time) ------------------------------------------
out vec3 vCol;
out vec2 vRad;   // x = radius in px, y = vertical squash (1 = round)

const vec3 GOLD = vec3(1., .62, .25), TEAL = vec3(.22, .68, 1.), HOT = vec3(1., .9, .75);
const vec3 LAT_N = vec3(128., 44., 10.); // lattice sites
const float LAT_A = .065;                // lattice constant

// --- scene 0/1: cloud collapsing into a cosmic web (Zel'dovich displacement) ---
vec3 scWeb(vec3 r, vec3 s, float t, out vec4 ce) {
  // grains drift slowly through the standing pattern of the potential
  vec3 q = (fract(r + t * (vec3(.012, .003, -.009) + (s - .5) * .008)) - .5) * vec3(9., 5.5, 9.);
  float kt = kickT(t);
  float D = .62 * smoothstep(4., 19.5, t) + .055 * exp(-kt * 4.) * smoothstep(8., 12., t);
  vec3 f = vec3(0, 0, t * .015);
  vec4 g = fbmd(q * .5 + f, .25);
  vec3 p = q - D * g.yzw * 2.;
  // grains that ended up in a potential well are in a dense filament: make them carry the light
  float well = fbmd(p * .5 + f, .25).x;
  float dens = smoothstep(.42, .2, well);
  float form = smoothstep(5., 18., t);
  vec3 c = mix(vec3(.1, .45, 1.), GOLD, dens);
  c = mix(vec3(.75, .8, .9), c, smoothstep(5., 12., t));     // colour arrives with the structure
  float e = mix(1., .5 + 2.2 * dens * dens, form);
  // kick: a shell of light travelling outwards
  e *= 1. + 1.6 * exp(-kt * 3.) * exp(-sq(length(p) - kt * 7.) * 1.5) * smoothstep(8., 9., t);
  // the plucked notes: grains flash in groups, at random at first, then on the 16th grid
  float st16 = floor(t * 8.), fl = step(.985, hf(uint(r.x * 4096.) + uint(st16) * 7919U)) * exp(-7. * fract(t * 8.)) * smoothstep(2., 6., t);
  e *= 1. + 14. * fl; c = mix(c, vec3(1., .95, .85), fl);
  e *= smoothstep(1.02, .72, length(q / vec3(4.5, 2.75, 4.5)));  // soft ellipsoid, no box edges
  p.xz *= rot(t * .02);
  ce = vec4(c, e);
  return p;
}

// --- scene 2: spiral galaxy ---------------------------------------------------
vec3 scGal(vec3 r, vec3 s, float t, out vec4 ce) {
  float grow = mix(.08, 1., smoothstep(T_GRAV - .3, T_GRAV + 2.4, t));
  float rho = -1.3 * log(1. - r.x * .968);
  float th = TAU * r.y + t * (.25 + .9 / (.5 + rho));          // stars drift through the arms
  float arm = 3.1 * log(rho + .2) + t * .5;                    // two-armed log-spiral density wave
  th -= .41 * sin(2. * (th - arm)) + (s.y - .5) * .3 * s.y;
  float den = .5 + .5 * cos(2. * (th - arm));
  vec3 p = vec3(rho * cos(th), 0., rho * sin(th));
  // flocculent structure: the same collapse law as the web, in the plane of the disc
  vec4 g = fbmd(vec3(p.xz * 2.2, 3.7) + vec3(0, 0, t * .02), .4);
  p.xz -= .055 * g.yz;
  float clump = smoothstep(.5, .25, g.x);
  p.y = (r.z + s.x - 1.) * (.05 + .04 * rho) * (1. - .5 * clump);
  vec3 c = mix(vec3(.95, .5, .3), vec3(.45, .68, 1.), den * (.4 + .6 * clump));
  c = mix(c, vec3(1., .78, .5), exp(-rho * 1.6));
  float e = (.3 + 1.5 * den * den) * (.5 + 1.2 * clump) * (1. + .6 * exp(-rho * 2.));
  if (s.z < .02 * den) { c = vec3(1., .3, .5); e *= 5. + 9. * exp(-fract(t) * 3.); }       // star-forming knots, glowing with the glass notes
  else if (s.z < .05) { c = vec3(.8, .9, 1.); e *= 5. * s.x * (1. + 4. * exp(-clapT(t) * 6.) * step(.5, fract(s.x * 7.))); }  // a few bright stars, twinkling on the claps
  if (s.z > .9) { // bulge
    vec3 d = normalize(r - .5 + (s - .5) * .3);
    p = d * (.03 + .75 * r.x * r.x) * vec3(1, .6, 1);
    c = vec3(1., .72, .4); e = .5;
  }
  p *= grow;
  e *= mix(.25, 1., grow);
  // kick: ring wave running outwards
  float kt = kickT(t);
  e *= 1. + 1.8 * exp(-kt * 2.5) * exp(-sq(rho - .4 - kt * 7.) * 2.5);
  ce = vec4(c, e);
  return p;
}

// --- scene 3: phyllotaxis disc wrapping into a Fibonacci sphere ----------------
vec3 scPhy(vec3 r, float t, vec3 ro, out vec4 ce) {
  const float M = 2600.;
  float k = floor(r.z * M);
  // divergence angle slightly off at first: many-armed spirals that lock into the golden angle
  float ga = 2.39996323 + .0095 * exp(-(t - T_SPH + .4) * 1.5);
  float th = k * ga + t * .15;
  float u = (k + .5) / M, rho = sqrt(u) * 2.7;
  // seed = small disc of grains, denser in the middle
  float sr = .037 * pow(r.x, .75), sa = TAU * r.y;
  vec3 pd = vec3(rho * cos(th), 0., rho * sin(th)) + sr * vec3(cos(sa), 0., sin(sa));
  // sphere: equal-area mapping of the same seeds
  float cz = 1. - 2. * u, sz = sqrt(max(1. - cz * cz, 0.));
  vec3 n = vec3(sz * cos(th), cz, sz * sin(th));
  vec3 tu = normalize(cross(n, vec3(.01, 1., .02))), tv = cross(n, tu);
  float beat = fract(t * 2. - u * 1.2);
  float R = 2.05 * (1. + .045 * exp(-5. * beat) * smoothstep(29., 30., t));
  vec3 ps = n * R + (tu * cos(sa) + tv * sin(sa)) * sr * 1.25;
  float w = smoothstep(28.2, 30.4, t - u * .9);
  vec3 p = mix(pd, ps, w);
  p.y += sin(w * PI) * .8 - w * .15;
  float pulse = exp(-5. * beat);
  // one family of spirals lights up per beat: 8, 13, 21, 34 arms (Fibonacci numbers)
  float bi = floor(t * 2.), fam = bi - 4. * floor(bi / 4.);
  float fn = fam < 1. ? 8. : fam < 2. ? 13. : fam < 3. ? 21. : 34.;
  float arm = step(mod(k + floor(bi * .25) * 3., fn), fn > 20. ? 2.5 : .5) * exp(-3. * fract(t * 2.)) * smoothstep(T_SPH + .8, T_SPH + 1.6, t);
  pulse = max(pulse * .6, arm);
  vec3 c = mix(mix(HOT, GOLD, smoothstep(0., .25, u)), TEAL, .75 * smoothstep(.5, 1., u) * (1. - w));
  c = mix(c, vec3(1., .95, .85), pulse * .75);
  // on the sphere: limb glow, far side dimmed
  float nv = dot(mix(vec3(0, 1, 0), n, w), normalize(ro - p));
  float sh = mix(1., (nv > 0. ? .45 + .55 * nv + 1.6 * pow(1. - nv, 4.) : .13), w);
  c = mix(c, TEAL, .6 * w * pow(1. - abs(nv), 3.));
  ce = vec4(c, (.75 + 2.4 * pulse) * sh * (1.3 - .6 * r.x));
  return p;
}

// --- scene 4: Chladni figures on a vibrating plate ----------------------------
float chl(vec2 p, vec3 m) { return cos(m.x * PI * p.x) * cos(m.y * PI * p.y) + m.z * cos(m.y * PI * p.x) * cos(m.x * PI * p.y); }
vec2 chlG(vec2 p, vec3 m) {
  return -PI * vec2(m.x * sin(m.x * PI * p.x) * cos(m.y * PI * p.y) + m.z * m.y * sin(m.y * PI * p.x) * cos(m.x * PI * p.y),
                    m.y * cos(m.x * PI * p.x) * sin(m.y * PI * p.y) + m.z * m.x * cos(m.y * PI * p.x) * sin(m.x * PI * p.y));
}
vec2 chlProj(vec2 p, vec3 m, float k) {
  vec2 q = p;
  for (int i = 0; i < 4; i++) { vec2 g = chlG(q, m); q = clamp(q - chl(q, m) * g / (dot(g, g) + .02), 0., 1.); }
  return mix(p, q, k);
}
vec3 scChl(vec3 r, vec3 s, float t, out vec4 ce) {
  const float S = 6.2;
  float x = t - T_RES, ti = floor(x), u0 = x - ti;
  int i = int(ti);
  vec2 q = r.xy, p;
  float hop = 0., e = 1.;
  vec3 c = GOLD;
  if (s.z < .8) { // sand: gathers on the nodal lines of the current mode
    float k = 1. - .55 * pow(s.x, 5.);
    vec2 pa = i > 0 ? chlProj(q, chMode(i - 1), k) : q, pb = i >= 0 ? chlProj(q, chMode(i), k) : q;
    float u = sat((u0 - s.y * .22) / .5), w = u * u * (3. - 2. * u);
    vec2 j = hf2(uint(r.x * 16777216.)) + hf2(uint(r.y * 16777216.)) - 1.;
    p = mix(pa, pb, w) + j * (.006 + .03 * s.x * s.x * s.x);
    // every strike throws the grains up; those sitting on a new antinode fly highest
    float kick = i >= 0 ? .05 + .45 * abs(chl(pa, chMode(i))) : 0.;
    hop = sin(u * PI) * kick * (.3 + s.x) * .8;
    float on = exp(-abs(chl(p, chMode(i))) * 9.);
    c = mix(GOLD * vec3(1., .85, .7), HOT, on * .8 + sat(hop * 4.));
    e = .55 + .6 * on;
  } else if (s.z < .975) { // shimmer: loose dust dancing where the plate moves
    p = q;
    float amp = i >= 0 ? abs(chl(q, chMode(i))) : 0.;
    hop = amp * .04 * abs(sin(t * 31. + s.x * 50.)) * (.3 + .7 * exp(-u0 * 1.2));
    c = TEAL; e = .5 + amp * (.9 + 3. * exp(-u0 * 2.5));
    // the strike runs outwards from the centre of the plate
    e *= 1. + 3. * exp(-sq(length(q - .5) - u0 * 1.6) * 60.) * step(0., x);
  } else { // rim of the plate
    float a = s.x * 4., b = fract(a);
    p = a < 1. ? vec2(b, 0) : a < 2. ? vec2(1, b) : a < 3. ? vec2(1. - b, 1) : vec2(0, 1. - b);
    p += (s.xy - .5) * .004;
    c = TEAL; e = .35 * (1. + 2. * exp(-u0 * 4.) * step(0., x));
  }
  ce = vec4(c, e);
  return vec3((p.x - .5) * S, hop, (p.y - .5) * S);
}

// --- scene 5: crystal lattice --------------------------------------------------
// site -> position; front layer at z = 0, the block extends backwards
vec3 latPos(vec3 site, vec3 r, vec3 s, float t, out vec4 ce) {
  vec3 p = vec3((site.xy - (LAT_N.xy - 1.) * .5) * LAT_A, -site.z * LAT_A);
  vec3 o = normalize(s - .5 + 1e-4) * .008 * (.25 + s.x);
  float e = 1.;
  if (r.z > .62) { // bond towards the next site along one axis
    int a = int(s.y * 3.);
    o = vec3(a == 0, a == 1, a == 2) * (s.z * LAT_A) + (s - .5) * .003;
    e = .22;
  }
  // heat: the grains arrive jittering and freeze into place
  float heat = exp(-max(t - T_LAT - .6, 0.) * 2.2);
  o += (hf3(uint(r.x * 16777216.) + uint(t * 30.) * 31U) - .5) * .09 * heat;
  // every kick sends a compression wave through the crystal
  float kt = kickT(t), ph = dot(p, vec3(1.1, .35, .6)) - kt * 9. + 4.;
  float wv = exp(-ph * ph * .8) * exp(-kt * 2.5);
  p += vec3(.02, .008, .012) * sin(ph * 6.) * wv;
  // the build makes the whole crystal ring
  float ri = riser(t);
  p += ri * ri * .014 * sin(vec3(31., 37., 29.) * t + p.yzx * 40.);
  // notes of the arpeggio light up single sites
  float st16 = floor(t * 8.), spark = step(.965, hf(uint(site.x + site.y * 131. + site.z * 17011.) + uint(st16) * 7919U)) * exp(-6. * fract(t * 8.));
  float rl = exp(-rollT(t) * 14.) * ri;
  ce = vec4(mix(GOLD, HOT, .25 + .7 * wv + spark + rl), e * (1. + 2.5 * wv + 5. * spark + .8 * ri * ri + 1.4 * rl));
  return p + o;
}
vec3 scLat(vec3 r, vec3 s, float t, out vec4 ce) {
  vec3 site = floor(vec3(r.xy, s.x) * LAT_N);
  vec3 p = latPos(site, r, s, t, ce);
  float fr = smoothstep(T_LAT + 2.6, T_LAT + 3.6, t);
  ce.a *= exp(-site.z * 1.1 * fr) * (1. + 2.5 * fr);
  return p;
}

// --- scene 6: the word -----------------------------------------------------------
// distance to segment, tracking the nearest centre-line point
void seg(vec2 p, vec2 a, vec2 b, inout float d, inout vec2 c) {
  vec2 pa = p - a, ba = b - a;
  vec2 n = a + ba * sat(dot(pa, ba) / dot(ba, ba));
  float l = distance(p, n);
  if (l < d) { d = l; c = n; }
}
void arc(vec2 p, vec2 o, float rad, inout float d, inout vec2 c) { // right half circle
  vec2 v = p - o;
  if (v.x > 0.) { vec2 n = o + normalize(v) * rad; float l = distance(p, n); if (l < d) { d = l; c = n; } }
}
// nearest point on the centre lines of "MORPH" (returns distance), word is 6.55 x 1.3, centred
const float LH = 1.3, LW = .13;
float sdWord(vec2 p, out vec2 c) {
  float d = 1e3;
  c = p;
  p += vec2(3.275, LH * .5);
  float y0 = LW, y1 = LH - LW, x0 = LW;
  vec2 o = vec2(0);
  if (p.x < 1.35) { // M (w 1.2)
    float x1 = 1.2 - LW;
    seg(p, vec2(x0, y0), vec2(x0, y1), d, c); seg(p, vec2(x0, y1), vec2(.6, .42), d, c);
    seg(p, vec2(.6, .42), vec2(x1, y1), d, c); seg(p, vec2(x1, y1), vec2(x1, y0), d, c);
  } else if (p.x < 2.8) { // O (w 1.15)
    o.x = 1.5; p.x -= o.x;
    vec2 e = vec2(1.15, LH) * .5 - LW, v = (p - vec2(.575, LH * .5)) / e;
    float l = length(v);
    vec2 n = vec2(.575, LH * .5) + v / max(l, 1e-4) * e;
    d = distance(p, n); c = n;
  } else if (p.x < 4.1) { // R (w 1.0)
    o.x = 2.95; p.x -= o.x;
    float ym = .6, rb = (y1 - ym) * .5, xb = 1. - LW - rb - .05;
    seg(p, vec2(x0, y0), vec2(x0, y1), d, c); seg(p, vec2(x0, y1), vec2(xb, y1), d, c); seg(p, vec2(x0, ym), vec2(xb, ym), d, c);
    arc(p, vec2(xb, (y1 + ym) * .5), rb, d, c); seg(p, vec2(xb - .05, ym), vec2(1. - LW, y0), d, c);
  } else if (p.x < 5.35) { // P (w .95)
    o.x = 4.25; p.x -= o.x;
    float ym = .6, rb = (y1 - ym) * .5, xb = .95 - LW - rb;
    seg(p, vec2(x0, y0), vec2(x0, y1), d, c); seg(p, vec2(x0, y1), vec2(xb, y1), d, c); seg(p, vec2(x0, ym), vec2(xb, ym), d, c);
    arc(p, vec2(xb, (y1 + ym) * .5), rb, d, c);
  } else { // H (w 1.05)
    o.x = 5.5; p.x -= o.x;
    float x1 = 1.05 - LW;
    seg(p, vec2(x0, y0), vec2(x0, y1), d, c); seg(p, vec2(x1, y0), vec2(x1, y1), d, c); seg(p, vec2(x0, LH * .5), vec2(x1, LH * .5), d, c);
  }
  c += o - vec2(3.275, LH * .5);
  return d;
}
vec3 scWord(vec3 r, vec3 s, float t, out vec4 ce) {
  vec3 site = floor(vec3(r.xy, s.x) * LAT_N);
  vec2 P = (site.xy - (LAT_N.xy - 1.) * .5) * LAT_A, c;
  float d = sdWord(P, c), dim = 1.;
  if (d > LW) {
    if (s.z < .62) { // most grains outside the strokes move to a lattice site inside the nearest stroke
      vec2 n = c + (P - c) / d * LW * (s.y * 2. - 1.) + (s.zx - .5) * .05;
      site.xy = floor(n / LAT_A + (LAT_N.xy - 1.) * .5 + .5);
      P = (site.xy - (LAT_N.xy - 1.) * .5) * LAT_A;
      d = sdWord(P, c);
    } else dim = .05;                                       // the rest stays behind: a faint crystal and a slow spiral of dust
  }
  if (dim == 1.) site.z = floor(s.y * s.y * 7.);              // letters: extruded a few layers deep, front layer densest
  vec3 p = latPos(site, r, s, t, ce);
  float edge = smoothstep(LW * .45, LW, d);
  float role = fract(s.x * 113.);
  if (dim == 1. && role > .55) { // part of the grains fill the strokes as a soft luminous body behind the lattice
    vec2 o = (vec2(s.z, fract(s.z * 91.7)) * 2. - 1.) * LW;
    vec2 n = c + o;
    float dd = sdWord(n, c);
    p = vec3(n, -.03 - s.x * .3);
    ce.a = dd < LW ? .55 : 0.;
    edge = 0.; site.z = 0.;
    if (role > .9) { // ... and a thin bright contour
      vec2 g = n - c;
      p = vec3(c + g / max(length(g), 1e-4) * (LW + .012), -fract(s.z * 37.3) * .32);
      ce.a = .5; edge = 1.;
    }
  }
  // sweep of light across the word on every kick
  float kt = kickT(t), sw = exp(-sq(p.x + 3.8 - kt * 17.) * .7) * exp(-kt * 3.);
  float depth = exp(-site.z * .5);
  float cl = exp(-clapT(t) * 5.);
  // light keeps travelling through the strokes
  float tr = 1. + .28 * sin(p.x * 2.6 + p.y * 1.7 - t * 5.);
  if (dim == 1. && edge == 1.) { // contour grains puff outwards on every kick
    vec2 g = normalize(p.xy - c + 1e-5);
    p.xy += g * (.25 + .9 * s.y) * kt * exp(-kt * 7.) * 1.6;
    p.z += (s.z - .5) * kt * exp(-kt * 7.) * 1.5;
  }
  ce = vec4(mix(mix(GOLD, HOT, .2 + .5 * edge), vec3(1), sw * .7), ce.a * dim * (dim == 1. ? depth * (.7 + .8 * edge) * (1. + 2.2 * sw) * tr : 1. + 5. * cl));
  if (dim < 1. && s.y > .45) { // spiral of dust turning slowly behind the word
    float rho = 1.2 + 4.2 * sqrt(r.x), th = TAU * r.y + t * (.9 / rho + .08);
    th -= .4 * sin(2. * (th - 2.6 * log(rho) - t * .25));
    p = vec3(rho * cos(th) * 1.25, rho * sin(th) * .8 + .2, -2.6 - 1.5 * s.x - .25 * rho);
    ce = vec4(mix(TEAL, vec3(.5, .6, 1.), s.x), 1.6 * (.4 + cl) * (.5 + .5 * cos(2. * (th - 2.6 * log(rho) - t * .25))));
  }
  return p;
}

void main() {
  uint id = uint(gl_VertexID);
  vec3 r = hf3(id * 3U), s = hf3(id * 3U + 1U);
  float h = hf(id * 3U + 2U);
  // motion blur for free: every grain lives a little in the past, each by its own fraction of the shutter time
  float t = TIME - fract(h * 733.) * U[2].x;
  vec4 ce, cb;
  vec3 p, pb;
  float w = 0.;
  vec3 ro, cu, cv, cw, cp;
  camera(t, ro, cu, cv, cw, cp);
  // --- which scene(s) ---
  if (t < T_GRAV + 1.6) {
    p = scWeb(r, s, t, ce);
    if (t > T_GRAV - 1.5) { pb = scGal(r, s, t, cb); w = sat((t - T_GRAV + 1.5 - length(p) * .1 - h * .35) / .9); }
  } else if (t < T_SPH + 1.6) {
    p = scGal(r, s, t, ce);
    if (t > T_SPH - .5) { pb = scPhy(r, t, ro, cb); w = sat((t - T_SPH + .5 - h * 1.1) / .9); }
  } else if (t < T_RES + .5) {
    p = scPhy(r, t, ro, ce);
    if (t > T_RES - .3) { pb = scChl(r, s, t, cb); w = sat((t - T_RES + .3 - h * .3) / .4); }
  } else if (t < T_LAT + 1.8) {
    p = scChl(r, s, t, ce);
    if (t > T_LAT - .2) { pb = scLat(r, s, t, cb); w = sat((t - T_LAT + .2 - r.x * .9 - h * .3) / .8); }
  } else if (t < T_MORPH + 2.2) {
    p = scLat(r, s, t, ce);
    if (t > T_MORPH) { pb = scWord(r, s, t, cb); w = sat((t - T_MORPH - r.x * .9 - h * .4) / .9); }
  } else {
    p = scWord(r, s, t, ce);
  }
  // glossy floor under the word: part of the grains draw its mirror image, dimmer and softer
  float refl = 0.;
  if (t > T_MORPH && h > .82) {
    refl = smoothstep(T_MORPH + .3, T_MORPH + 2., t);
    p.y = mix(p.y, -1.7 - p.y + .012 * sin(p.x * 23. + t * 4.), refl);
    ce.a *= mix(1., .9 * exp(-max(-.85 - p.y, 0.) * 2.2), refl);
  }
  if (w > 0.) {
    float e = w * w * (3. - 2. * w);
    p = mix(p, pb, e);
    ce = mix(ce, cb, e);
  }
  vec3 col = ce.rgb;
  float en = ce.a;
  // --- ambient dust near the camera and far background stars (a few percent of the grains, always there) ---
  float far = 0., glint = 0.;
  if (h < .05) {
    if (h < .03) {
      p = (fract(r + t * (s - .5) * .012) - .5) * vec3(13., 7., 13.);
      col = vec3(.8, .75, .7); en = .5;
    } else {
      p = normalize(r - .5) * (30. + 20. * s.x);
      float b = s.z * s.z * s.z;
      col = mix(vec3(1., .8, .6), vec3(.6, .8, 1.), s.y); en = 1400. * b * b * (.6 + .4 * sin(t * 3. + s.x * 40.));
      far = 1.; glint = step(.985, s.z); en *= 1. + 9. * glint;
    }
  }
  // --- outro: CRT switch-off collapse ---
  float oc = t - T_OUT;
  if (oc > 0.) {
    p.y = mix(p.y, -.12, 1. - exp(-oc * 22.));
    p.x *= exp(-max(oc - .25, 0.) * 14.);
    p.z *= exp(-oc * 8.);
    en *= mix(1., .06, sat(oc * 7.)) * exp(-max(oc - .55, 0.) * 6.) * step(oc, 1.5);
    col = mix(col, vec3(.85, .92, 1.), sat(oc * 5.));
  }

  vec3 v = p - ro;
  float z = dot(v, cw);
  vec2 sp = vec2(dot(v, cu), dot(v, cv)) * cp.x / max(z, 1e-3);
  // depth of field: circle of confusion in pixels; big discs are thinned stochastically (energy preserved)
  float coc = cp.z * abs(z - cp.y) / max(z, .05) * RES.y * .5 * cp.x * (1. - .9 * far);
  float rad = sqrt(1.2 * 1.2 + coc * coc + refl * 9.);
  float e = en * expo(t) / max(z * z, .08) * exp(-z * fogD(t) * (1. - far)) * smoothstep(.12, .5, z);
  float keep = min(1., 16. / (rad * rad)), hk = fract(h * 61.);
  e *= smoothstep(0., .3, 1. - hk / keep) / keep;
  rad = min(rad, 48.);
  vec2 squash = vec2(rad, 1.);
  vec4 pos = (z > .05 && hk < keep) ? vec4(sp.x * RES.y / RES.x, sp.y, 0, 1) : vec4(2, 2, 2, 1);
  // --- TV snow: a grain is either "noise" (random on screen, new place every 1/30 s) or already settled ---
  float pn = 1. - smoothstep(.9, 7.2, t + (fract(h * 977.) - .5) * 1.5);
  if (fract(h * 977.) < pn) {
    uint tick = uint(t * 30.);
    vec3 sn = hf3(id + tick * 7919U);
    vec2 a = hf2(id * 5U + tick * 104729U);
    // rolling bars: a brighter band drifting down, and a darker one
    float bar = 1. + 1.3 * exp(-sq((fract(a.y + t * .23) - .5) * 9.)) - .6 * exp(-sq((fract(a.y - t * .11 + .3) - .5) * 5.));
    pos = sn.y < .028 ? vec4(a * 2. - 1., 0, 1) : vec4(2, 2, 2, 1);
    squash = vec2(1.3 + 3.5 * sn.z * sn.z * sn.z, .32);
    col = vec3(.82, .88, 1.);
    e = 1.1 * (.08 + sn.x * sn.x) * bar * smoothstep(0., .8, t);
    rad = squash.x;
  }
  vCol = col * e / (rad * rad * squash.y);
  vRad = squash;
  if (glint > 0. && rad < 4.) { vRad = vec2(26., -rad); rad = 26.; }   // bright stars get diffraction spikes
  gl_PointSize = 2. * rad + 1.;
  gl_Position = pos;
}
