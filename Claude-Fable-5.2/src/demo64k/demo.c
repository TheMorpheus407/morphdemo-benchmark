/* MorphDemo 64k - "GRAINS": eight million stateless particles tell FROM NOISE TO ORDER.
 *
 * Every grain's position is a pure function of (particle id, time), evaluated in
 * the vertex shader (pvs.glsl): TV snow -> cloud collapsing into a cosmic web ->
 * spiral galaxy -> phyllotaxis disc / Fibonacci sphere -> Chladni figures on a
 * plate -> crystal lattice -> the word MORPH -> picture-tube switch-off.
 * Per frame:
 *   1. particles, additive, into an RGBA16F buffer (bokeh depth of field, stochastic
 *      motion blur), plus the dot-matrix captions (ovl.glsl)
 *   2. bloom: 6-level downsample chain, tent-upsampled back (down.glsl / up.glsl)
 *   3. anamorphic streak and radial light shafts from the quarter-resolution level
 *   4. composite: aberration, tube distortion while the signal is noise, flashes,
 *      tone mapping, grade, grain (post.glsl)
 * The soundtrack is a sound shader (snd.glsl) rendered once on the GPU into four
 * stereo stems (dry / reverb send / delay send / hall send) and finished on the
 * CPU (ping-pong delay, plate-like FDN, 16-line hall, look-ahead limiter).
 * Picture and sound read the same score (score.glsl), so they are in sync by
 * construction.  This file only wires the passes together.                     */
#include "../common/sys.h"
#include "../common/gl.h"
#include "../common/plat_imports.h"
#include "../common/eng_imports.h"
IMP_END
#include "../common/plat.c"
#define DEMO_LEN 58.0
#define NU 8
#include "../common/eng.c"
#include "../common/fx.c"
#include "fx2.c"

#if defined(DEV) && !defined(USE_GEN)
#define CH(id, file) static const char id[] = file;
#include "chunks.def"
#else
#include "shaders_gen.h"
#endif
static const char *const vs_quad[] = {c_hdr, c_vs, 0};
static const char *const vs_part[] = {c_hdr, c_score, c_cam, c_pvs, 0};
static const char *const fs_part[] = {c_hdr, c_pfs, 0};
static const char *const fs_down[] = {c_hdr, c_down, 0};
static const char *const fs_up[] = {c_hdr, c_up, 0};
static const char *const fs_ovl[] = {c_hdr, c_score, c_font, c_ovl, 0};
static const char *const fs_streak[] = {c_hdr, c_streak, 0};
static const char *const fs_rays[] = {c_hdr, c_rays, 0};
static const char *const fs_post[] = {c_hdr, c_score, c_post, 0};
static const char *const fs_snd[] = {c_hdr, c_score, c_snd, 0};

#define NPART (1 << 23)
#define NBLOOM 6
#define GL_POINT_SPRITE 0x8861

static GLuint p_part, p_ovl, p_down, p_up, p_streak, p_rays, p_post, p_snd;
static RT rt_hdr, rt_b[NBLOOM], rt_streak, rt_rays, rt_out;
static float st[4][AU_LEN_FRAMES * 2];

static void demo_init(void) {
  int i;
  p_part = eng_prog(vs_part, fs_part, 0);
  p_ovl = eng_prog(vs_quad, fs_ovl, 0);
  p_streak = eng_prog(vs_quad, fs_streak, 0);
  p_rays = eng_prog(vs_quad, fs_rays, 0);
  p_down = eng_prog(vs_quad, fs_down, 0);
  p_up = eng_prog(vs_quad, fs_up, 0);
  p_post = eng_prog(vs_quad, fs_post, 0);
  p_snd = eng_prog(vs_quad, fs_snd, 0);
  rt_make(&rt_hdr, RW, RH, GL_RGBA16F, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
  for (i = 0; i < NBLOOM; i++) rt_make(&rt_b[i], RW >> (i + 1), RH >> (i + 1), GL_RGBA16F, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
  rt_make(&rt_streak, RW / 4, RH / 4, GL_RGBA16F, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
  rt_make(&rt_rays, RW / 4, RH / 4, GL_RGBA16F, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
  rt_make(&rt_out, RW, RH, GL_RGBA8, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
}

static GLuint demo_frame(double t) {
  int i;
  eng_u[8] = 1.f / 48.f; /* shutter time (s) */
#ifdef DEV
  for (i = 0; i < 8; i++) { char nm[4] = {'G', '0' + i, 0}; const char *e = getenv(nm); if (e) eng_u[8 + i] = atof(e); }
#endif
  /* particles */
  rt_bind(&rt_hdr);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_POINT_SPRITE);
  eng_use(p_part);
  glDrawArrays(GL_POINTS, 0, NPART);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); /* captions go into the HDR buffer too (they bloom), over a dark plate */
  eng_use(p_ovl);
  eng_tri();
  glBlendFunc(GL_ONE, GL_ONE);
  glDisable(GL_BLEND);
  /* bloom: downsample chain, then tent-upsample back up accumulating */
  for (i = 0; i < NBLOOM; i++) {
    const RT *src = i ? &rt_b[i - 1] : &rt_hdr;
    rt_bind(&rt_b[i]);
    eng_tex(0, src->tex[0]);
    eng_u[4] = 1.f / src->w; eng_u[5] = 1.f / src->h; eng_u[6] = i ? 0.f : 1.f;
    eng_use(p_down);
    eng_tri();
  }
  glEnable(GL_BLEND);
  for (i = NBLOOM - 2; i >= 0; i--) {
    rt_bind(&rt_b[i]);
    eng_tex(0, rt_b[i + 1].tex[0]);
    eng_u[4] = 1.f / rt_b[i + 1].w; eng_u[5] = 1.f / rt_b[i + 1].h; eng_u[7] = 1.f;
    eng_use(p_up);
    eng_tri();
  }
  glDisable(GL_BLEND);
  /* anamorphic streak from the quarter-resolution level */
  rt_bind(&rt_streak);
  eng_tex(0, rt_b[1].tex[0]);
  eng_u[4] = 1.f / rt_streak.w; eng_u[5] = 1.f / rt_streak.h;
  eng_use(p_streak);
  eng_tri();
  /* light shafts */
  rt_bind(&rt_rays);
  eng_u[6] = 0.f;
  eng_use(p_rays);
  eng_tri();
  /* composite */
  rt_bind(&rt_out);
  eng_tex(3, rt_rays.tex[0]);
  eng_tex(2, rt_streak.tex[0]);
  eng_tex(1, rt_b[0].tex[0]);
  eng_tex(0, rt_hdr.tex[0]);
  eng_use(p_post);
  eng_tri();
  return rt_out.fbo;
}

static void demo_audio(float *buf, int n) {
  float *stems[4] = {st[0], st[1], st[2], st[3]};
  int i;
  eng_sound(p_snd, 2, stems, n);                 /* 0 dry, 1 reverb send, 2 delay send, 3 hall send */
#ifdef DEV
  if (getenv("D64_STEMS")) for (i = 0; i < 4; i++) { char fn[256]; snprintf(fn, sizeof fn, "%s%d.wav", getenv("D64_STEMS"), i); dev_wav(fn, st[i], n); }
#endif
  for (i = 0; i < 2 * n; i++) buf[i] = st[0][i];
  fx_delay(st[2], st[1], n, 18000, 0.42f, 0.6f);  /* dotted-eighth ping-pong, feeding the reverb */
  fx_delay(st[2], buf, n, 18000, 0.42f, 0.55f);
  fx_reverb(st[1], buf, n, 0.84f, 0.3f, 0.26f);
  fx_hall(st[3], buf, n, 4.2f, 0.35f, 0.28f);
  fx_master(buf, n, 0.8f);
  for (i = 0; i < 2 * n; i++) buf[i] *= 0.78f;    /* keeps the true peak below -1 dBFS */
}
