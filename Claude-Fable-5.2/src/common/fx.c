/* Audio helpers for the C demos:
 *   eng_sound()  render a "sound shader" on the GPU into float stems
 *   fx_reverb()  8-line feedback-delay-network reverb (Householder mixing)
 *   fx_delay()   stereo ping-pong delay
 *   fx_master()  look-ahead peak limiter
 * All buffers are interleaved stereo float, AU_RATE Hz.                      */
#ifndef FX_C
#define FX_C

#define SND_W 2048

/* Draws `prog` over a SND_W x ceil(n/SND_W) float target with `nrt` RGBA32F
 * attachments; pixel i holds sample frame i: attachment k = stems 2k (rg) and
 * 2k+1 (ba).  stems[j] receives 2*n floats.                                   */
static void eng_sound(GLuint prog, int nrt, float **stems, int n) {
  static float tmp[SND_W * 4 * 64];
  RT rt;
  int h = (n + SND_W - 1) / SND_W, k, y, i;
  rt_make(&rt, SND_W, h, GL_RGBA32F, nrt, 1, GL_NEAREST, GL_CLAMP_TO_EDGE);
  rt_bind(&rt);
  eng_use(prog);
  glEnable(GL_SCISSOR_TEST);
  for (y = 0; y < h; y += 64) { /* banded so that no single draw call runs for long */
    glScissor(0, y, SND_W, 64);
    eng_tri();
    glFinish();
  }
  glDisable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, rt.fbo);
  for (k = 0; k < nrt; k++) {
    glReadBuffer(GL_COLOR_ATTACHMENT0 + k);
    for (y = 0; y < h; y += 64) {
      int rows = h - y < 64 ? h - y : 64, cnt = rows * SND_W, base = y * SND_W;
      glReadPixels(0, y, SND_W, rows, GL_RGBA, GL_FLOAT, tmp);
      for (i = 0; i < cnt && base + i < n; i++) {
        stems[2 * k][2 * (base + i)] = tmp[4 * i];
        stems[2 * k][2 * (base + i) + 1] = tmp[4 * i + 1];
        stems[2 * k + 1][2 * (base + i)] = tmp[4 * i + 2];
        stems[2 * k + 1][2 * (base + i) + 1] = tmp[4 * i + 3];
      }
    }
  }
  glReadBuffer(GL_COLOR_ATTACHMENT0);
}

/* FDN reverb.  in: send (stereo), out: wet is ADDED to out.  decay ~ feedback
 * gain per pass (0..1), damp = one-pole low-pass coefficient (0 = bright).    */
static void fx_reverb(const float *in, float *out, int n, float decay, float damp, float wet) {
  static const int len[8] = {1423, 1637, 1873, 2087, 2311, 2543, 2777, 3011};
  static const int ap[4] = {223, 367, 491, 613};
  static float d[8][3072], a[4][2][640];
  float lp[8], v[8];
  int i, j, c, p = 0;
  for (j = 0; j < 8; j++) { lp[j] = 0; for (i = 0; i < 3072; i++) d[j][i] = 0; }
  for (j = 0; j < 4; j++) for (c = 0; c < 2; c++) for (i = 0; i < 640; i++) a[j][c][i] = 0;
  for (i = 0; i < n; i++, p++) {
    float x[2], s = 0;
    for (c = 0; c < 2; c++) { /* input diffusion: 4 series all-passes per channel */
      float y = in[2 * i + c];
      for (j = 0; j < 4; j++) {
        float *b = a[j][c]; int l = ap[j] + c * 23, q = p % l; float z = b[q], w = y + z * 0.5f;
        b[q] = w; y = z - w * 0.5f;
      }
      x[c] = y;
    }
    for (j = 0; j < 8; j++) { v[j] = d[j][p % len[j]]; s += v[j]; }
    s *= 0.25f; /* Householder: v - 2/N * sum */
    for (j = 0; j < 8; j++) {
      float f = (v[j] - s) * decay;
      lp[j] += (f - lp[j]) * (1.f - damp);
      d[j][p % len[j]] = lp[j] + x[j & 1];
    }
    out[2 * i] += wet * (v[0] - v[2] + v[4] - v[6]);
    out[2 * i + 1] += wet * (v[1] - v[3] + v[5] - v[7]);
  }
}

/* Ping-pong delay: `len` frames per tap, feedback fb, low-pass in the loop. */
static void fx_delay(const float *in, float *out, int n, int len, float fb, float wet) {
  static float b[2][65536];
  float lp0 = 0, lp1 = 0;
  int i;
  for (i = 0; i < 65536; i++) b[0][i] = b[1][i] = 0;
  for (i = 0; i < n; i++) {
    int q = i % len;
    float l = b[0][q], r = b[1][q];
    lp0 += (r * fb - lp0) * 0.6f; lp1 += (l * fb - lp1) * 0.6f;
    b[0][q] = in[2 * i] + in[2 * i + 1] + lp0;
    b[1][q] = lp1;
    out[2 * i] += wet * l; out[2 * i + 1] += wet * r;
  }
}

/* Look-ahead limiter (5 ms) with gain `pre`, ceiling 0.97; also removes DC. */
static void fx_master(float *b, int n, float pre) {
  enum { LA = 240 };
  float env = 1, g = 1, dcl = 0, dcr = 0, xl = 0, xr = 0;
  int i, j;
  for (i = 0; i < n; i++) { /* DC blocker + gain */
    float l = b[2 * i] * pre, r = b[2 * i + 1] * pre;
    dcl = l - xl + 0.9995f * dcl; xl = l; dcr = r - xr + 0.9995f * dcr; xr = r;
    b[2 * i] = dcl; b[2 * i + 1] = dcr;
  }
  for (i = 0; i < n; i++) {
    float pk = 0, t;
    j = i + LA < n ? i + LA : n - 1;
    pk = b[2 * j] < 0 ? -b[2 * j] : b[2 * j];
    t = b[2 * j + 1] < 0 ? -b[2 * j + 1] : b[2 * j + 1];
    if (t > pk) pk = t;
    env = pk > env ? pk : env + (pk - env) * 0.0005f;            /* peak hold with slow release */
    t = env > 0.97f ? 0.97f / env : 1.f;
    g += (t - g) * (t < g ? 0.02f : 0.0008f);                      /* fast attack inside look-ahead */
    b[2 * i] *= g; b[2 * i + 1] *= g;
    if (b[2 * i] > 1) b[2 * i] = 1; if (b[2 * i] < -1) b[2 * i] = -1;
    if (b[2 * i + 1] > 1) b[2 * i + 1] = 1; if (b[2 * i + 1] < -1) b[2 * i + 1] = -1;
  }
}

#endif
