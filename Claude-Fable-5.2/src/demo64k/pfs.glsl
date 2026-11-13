in vec3 vCol;
in vec2 vRad;   // x = radius (px), y = vertical squash, or < 0: a star with diffraction spikes (|y| = core radius)
out vec4 o;
void main() {
  vec2 q = (gl_PointCoord - .5) * (2. * vRad.x + 1.);
  if (vRad.y < 0.) {
    float c = -vRad.y, d = length(q);
    vec2 a = abs(q);
    float w = clamp(c - d + .5, 0., 1.) + .12 * (exp(-a.y * 1.6 - a.x * .16) + exp(-a.x * 1.6 - a.y * .16)) * smoothstep(vRad.x, vRad.x * .5, d);
    o = vec4(vCol * w, 1);
    return;
  }
  q.y /= vRad.y;
  float d = length(q);
  float w = clamp(vRad.x - d + .5, 0., 1.);
  o = vec4(vCol * w, 1);
}
