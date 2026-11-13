// 3x3 tent upsample, blended additively into the next larger level (U[1].xy = source texel size, U[1].w = weight)
layout(binding=0) uniform sampler2D tex;
out vec4 o;
void main() {
  vec2 px = U[1].xy, uv = gl_FragCoord.xy * .5 * px;
  vec3 s = texture(tex, uv).rgb * 4.;
  s += (texture(tex, uv + px * vec2(-1, 0)).rgb + texture(tex, uv + px * vec2(1, 0)).rgb + texture(tex, uv + px * vec2(0, -1)).rgb + texture(tex, uv + px * vec2(0, 1)).rgb) * 2.;
  s += texture(tex, uv + px * vec2(-1, -1)).rgb + texture(tex, uv + px * vec2(1, -1)).rgb + texture(tex, uv + px * vec2(-1, 1)).rgb + texture(tex, uv + px * vec2(1, 1)).rgb;
  o = vec4(s * (U[1].w / 16.), 1);
}
