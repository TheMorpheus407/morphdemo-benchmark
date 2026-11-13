// light shafts: radial blur of the bright parts towards the picture centre (quarter resolution)
layout(binding=0) uniform sampler2D tex;
out vec4 o;
void main() {
  vec2 uv = gl_FragCoord.xy * U[1].xy, d = (vec2(.5, .5 + U[1].z) - uv) * (.85 / 36.);
  vec3 s = vec3(0);
  float w = 1.;
  uv += d * hf(uint(gl_FragCoord.x) + uint(gl_FragCoord.y) * 480U + uint(U[0].w) * 129600U);
  for (int i = 0; i < 36; i++) {
    s += max(texture(tex, uv).rgb - .3, 0.) * w;
    w *= .93;
    uv += d;
  }
  o = vec4(s / 13., 1);
}
