/* Skeleton demo: template for the 16k / 64k productions. */
#include "../common/sys.h"
#include "../common/gl.h"
#include "../common/plat_imports.h"
#include "../common/eng_imports.h"
IMP_END
#include "../common/plat.c"
#define DEMO_LEN 8.0
#include "../common/eng.c"
#include "../common/fx.c"

#if defined(DEV) && !defined(USE_GEN)
#define CH(id, file) static const char id[] = file;
#include "chunks.def"
#else
#include "shaders_gen.h"
#endif
static const char *const vs_quad[] = {c_hdr, c_vs, 0};
static const char *const fs_img[] = {c_hdr, c_img, 0};
static const char *const fs_snd[] = {c_hdr, c_snd, 0};

static GLuint p_img, p_snd;
static RT rt_main;
static float st_dry[AU_LEN_FRAMES * 2], st_send[AU_LEN_FRAMES * 2];

static void demo_init(void) {
  p_img = eng_prog(vs_quad, fs_img, 0);
  p_snd = eng_prog(vs_quad, fs_snd, 0);
  rt_make(&rt_main, RW, RH, GL_RGBA8, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
}

static GLuint demo_frame(double t) {
  rt_bind(&rt_main);
  eng_use(p_img);
  eng_tri();
  return rt_main.fbo;
}

static void demo_audio(float *buf, int n) {
  float *stems[2] = {st_dry, st_send};
  eng_sound(p_snd, 1, stems, n);
  for (int i = 0; i < 2 * n; i++) buf[i] = st_dry[i];
  fx_reverb(st_send, buf, n, 0.82f, 0.25f, 0.35f);
  fx_master(buf, n, 1.0f);
}
