// horizontal anamorphic streak of the brightest parts (source: quarter-resolution bloom level)
layout(binding=0) uniform sampler2D tex;
out vec4 o;
void main() {
  vec2 uv = gl_FragCoord.xy * U[1].xy;
  vec3 s = vec3(0);
  float ws = 0.;
  for (int i = -40; i <= 40; i++) {
    float w = exp(-float(i * i) / 420.);
    vec3 c = texture(tex, uv + vec2(float(i) * 1.6 * U[1].x, 0)).rgb;
    s += max(c - 1.6, 0.) * w;
    ws += w;
  }
  o = vec4(s / ws, 1);
}
