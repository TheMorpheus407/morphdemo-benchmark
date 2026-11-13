// Gray-Scott reaction-diffusion step.  x = substrate u, y = activator v.
// U[1].x = step index; U[2] = (feed, kill, dt, noise); U[3] = (seed rate, kill variation, text wipe, kick wave)
layout(binding=0) uniform sampler2D S;
out vec4 o;
vec2 T(ivec2 p) { ivec2 z = ivec2(SIM); return texelFetch(S, (p + z) % z, 0).xy; }
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  vec2 w = gl_FragCoord.xy / 60.;
  float s = U[1].x, t = s / SPS;
  if (s < .5) { o = vec4(1, 0, 0, 1); return; }
  vec2 c = T(p), cE = T(p + ivec2(1, 0)), cW = T(p - ivec2(1, 0)), cN = T(p + ivec2(0, 1)), cS = T(p - ivec2(0, 1));
  vec2 cNE = T(p + ivec2(1, 1)), cSW = T(p - ivec2(1, 1)), cSE = T(p + ivec2(1, -1)), cNW = T(p - ivec2(1, -1));
  vec2 l = .2 * (cE + cW + cN + cS) + .05 * (cNE + cSW + cSE + cNW) - c;
  // anisotropic diffusion of the activator: stripes comb themselves along direction g
  vec2 g = vec2(cos(U[1].w), sin(U[1].w));
  l.y += U[1].y * .25 * ((g.x * g.x - g.y * g.y) * (cE.y + cW.y - cN.y - cS.y) + g.x * g.y * (cNE.y + cSW.y - cNW.y - cSE.y));
  // slow drift across the stripes
  c -= U[2].z * U[1].z * .5 * (g.x * (cN - cS) - g.y * (cE - cW));
  float f = U[2].x, k = U[2].y, dt = U[2].z;
  // slow spatial drift of the kill rate: neighbouring regions sit in slightly different regimes
  k += U[3].y * (sin(w.x * .8 + t * .11) * sin(w.y * 1.1 - t * .07) + .5 * sin(w.x * 1.9 - w.y * 1.3 + t * .2));
  // the word: outside the letters the culture is starved (U[3].z: radius of the dead zone, growing from the centre)
  float m = smoothstep(.44, .32, textD(w));
  k += (1. - m) * .02 * smoothstep(0., 1., U[3].z - length(w - vec2(8, 4.5)) + sin(w.x * 3.) * .3);
  float r = c.x * c.y * c.y;
  c += dt * vec2(l.x - r + f * (1. - c.x), .5 * l.y + r - (f + k) * c.y);
  // thermal noise
  c.y += dt * U[2].w * hf(uint(p.x + p.y * 960) * 7919U + uint(s) * 104729U);
  // random nucleation: one candidate seed per 40x40 cell and 0.1 s slot
  vec2 gc = floor(gl_FragCoord.xy / 40.);
  uint e = uint(gc.x + gc.y * 24. + floor(t * 10.) * 337.) * 5U;
  if (hf(e) < U[3].x && fract(t * 10.) < .012)
    c.y += .3 * smoothstep(4., 2., length(gl_FragCoord.xy - (gc + .15 + .7 * vec2(hf(e + 1U), hf(e + 2U))) * 40.));
  // droplet notes seed the culture where they "land"
  vec4 d = drop(int(floor(t / S16)));
  if (d.x >= 0. && t >= d.x && t < d.x + .0012) {
    vec2 q = abs(w - d.zw); q = min(q, vec2(16, 9) - q);
    c.y += .3 * smoothstep(.08, .04, length(q));
  }
  // every kick sends a starvation ring through the tissue (from the centre outwards)
  float ka = kickAge(t), rr = length(w - U[4].xy) - ka * 7.;
  c.x *= 1. - dt * U[3].w * exp(-rr * rr * 6. - ka * 1.5);
  o = vec4(clamp(c, 0., 1.), 0, 1);
}
