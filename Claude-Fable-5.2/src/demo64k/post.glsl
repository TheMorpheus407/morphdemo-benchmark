// ---- composite: bloom, streaks, aberration, tone mapping, grain ----------------------
layout(binding=0) uniform sampler2D tHdr;
layout(binding=1) uniform sampler2D tBloom;
layout(binding=2) uniform sampler2D tStreak;
layout(binding=3) uniform sampler2D tRays;
out vec4 o;
vec3 tonemap(vec3 x) { return clamp((x * (2.51 * x + .03)) / (x * (2.43 * x + .59) + .14), 0., 1.); }
void main() {
  float t = TIME;
  vec2 uv = gl_FragCoord.xy / RES, c = uv - .5;
  // picture-tube feel while the signal is still noise (and again at switch-off): barrel distortion, scan lines
  float crt = (1. - smoothstep(3., 8.5, t)) + smoothstep(T_OUT - .1, T_OUT + .1, t);
  float r2 = dot(c, c);
  c *= 1. + crt * .09 * r2;
  // sync loss: the picture tears sideways in bands and jumps
  vec2 gl = glitch(t);
  float band = floor(c.y * (6. + 5. * gl.y) + gl.y * 1.7);
  c.x += gl.x * .12 * (hf(uint(band + 40.) + uint(gl.y) * 13U) - .5) * crt;
  c.y += gl.x * .03 * crt;
  uv = c + .5;
  r2 = dot(c, c);
  vec3 col = vec3(0);
  // radial chromatic aberration
  vec2 d = c * r2 * .008;
  col.r = texture(tHdr, uv + d).r; col.g = texture(tHdr, uv).g; col.b = texture(tHdr, uv - d).b;
  vec3 bl = vec3(texture(tBloom, uv + d * 2.).r, texture(tBloom, uv).g, texture(tBloom, uv - d * 2.).b);
  col += bl * .1;
  col += texture(tStreak, uv).rgb * vec3(.35, .6, 1.) * .3;
  // light shafts: strongest at the implosion, around the word and at switch-off
  float ra = .12 + .5 * exp(-sq(t - T_GRAV) * 1.2) + (.07 + .22 * kickEnv(t)) * smoothstep(T_MORPH, T_MORPH + 2., t) + .25 * smoothstep(T_LAT + 2.5, T_MORPH, t);
  ra *= 1. - .85 * smoothstep(T_OUT, T_OUT + .15, t);
  col += texture(tRays, uv).rgb * ra;
  // impacts on the big scene changes
  float fl = exp(-max(t - T_GRAV, 0.) * 5.) * step(T_GRAV, t) * .5 + exp(-max(t - T_MORPH, 0.) * 4.) * step(T_MORPH, t) * .9 + exp(-max(t - T_RES, 0.) * 6.) * step(T_RES, t) * .25;
  col += fl * (bl * .5 + .04) * vec3(1., .92, .8);
  col *= smoothstep(1.35, .25, length(c * vec2(1., .82)) * (1. + .5 * crt));               // vignette
  col *= 1. - crt * .22 * (.5 + .5 * cos(PI * gl_FragCoord.y));                           // scan lines
  vec2 e = smoothstep(.5, .49, abs(c));
  col *= e.x * e.y;                                                                        // edge of the tube
  col *= 1. + 1.5 * gl.x * crt;
  col *= 1. - .93 * smoothstep(43.75, 43.8, t) * step(t, T_MORPH);                         // breath held before the drop
  col = tonemap(col);
  // grade: colour arrives with the structure; a touch of teal in the shadows, warmth in the lights
  float l = dot(col, vec3(.25, .6, .15));
  col = mix(vec3(l), col, mix(.25, 1.08, smoothstep(3., 10., t)));
  col = pow(col, vec3(1.04, 1., .96));
  col = pow(col, vec3(1. / 2.2));
  col *= smoothstep(0., .6, t) * (1. - smoothstep(T_END - .9, T_END - .45, t));           // fade in / out
  // grain + dither
  uint n = uint(gl_FragCoord.x) + uint(gl_FragCoord.y) * 1920U + uint(U[0].w) * 2073600U;
  vec3 gr = hf3(n) - .5;
  col += gr * (.028 * (1. - .6 * l) * smoothstep(T_END - .45, T_END - .9, t) + 1. / 255.);
  o = vec4(col, 1);
}
