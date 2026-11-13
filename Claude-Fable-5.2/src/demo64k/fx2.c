/* 64k-only audio post: 16-line feedback delay network "hall" (Hadamard mixing,
 * per-line damping, pre-delay, all-pass input diffusion).  Wet is ADDED to out. */
#ifndef FX2_C
#define FX2_C
static void fx_hall(const float *in, float *out, int n, float rt60, float damp, float wet) {
  static const int len[16] = {1103, 1327, 1549, 1783, 2011, 2269, 2503, 2741, 2999, 3251, 3511, 3769, 4001, 4253, 4507, 4759};
  static const int apl[4] = {347, 113, 37, 211};
  static float d[16][4800], ap[2][4][400], pre[2][1024];
  float g[16], lp[16], v[16];
  int pos[16], app[4], i, j, c, pp = 0;
  for (j = 0; j < 16; j++) {
    float x = 6.9078f * len[j] / (rt60 * 48000.f), e = 1.f - x / 16.f;
    e *= e; e *= e; e *= e; e *= e; /* ~exp(-x) */
    g[j] = e; lp[j] = 0; pos[j] = 0;
    for (i = 0; i < 4800; i++) d[j][i] = 0;
  }
  for (j = 0; j < 4; j++) { app[j] = 0; for (c = 0; c < 2; c++) for (i = 0; i < 400; i++) ap[c][j][i] = 0; }
  for (c = 0; c < 2; c++) for (i = 0; i < 1024; i++) pre[c][i] = 0;
  for (i = 0; i < n; i++) {
    float x[2], l = 0, r = 0;
    for (c = 0; c < 2; c++) {
      float y = pre[c][pp];
      pre[c][pp] = in[2 * i + c];
      for (j = 0; j < 4; j++) { float *b = ap[c][j]; float z = b[app[j]], w = y + z * 0.6f; b[app[j]] = w; y = z - w * 0.6f; }
      x[c] = y;
    }
    pp = (pp + 1) & 1023;
    for (j = 0; j < 4; j++) if (++app[j] >= apl[j] + 0) app[j] = 0;
    for (j = 0; j < 16; j++) v[j] = d[j][pos[j]];
    for (j = 0; j < 16; j += 4) { l += v[j] - v[j + 2]; r += v[j + 1] - v[j + 3]; }
    /* fast Walsh-Hadamard transform, normalised by 1/4 */
    for (c = 1; c < 16; c <<= 1)
      for (j = 0; j < 16; j += c << 1) { int k; for (k = j; k < j + c; k++) { float a = v[k], b = v[k + c]; v[k] = a + b; v[k + c] = a - b; } }
    for (j = 0; j < 16; j++) {
      lp[j] += (v[j] * 0.25f * g[j] - lp[j]) * (1.f - damp);
      d[j][pos[j]] = lp[j] + x[j & 1];
      if (++pos[j] >= len[j]) pos[j] = 0;
    }
    out[2 * i] += wet * l; out[2 * i + 1] += wet * r;
  }
}
#endif
