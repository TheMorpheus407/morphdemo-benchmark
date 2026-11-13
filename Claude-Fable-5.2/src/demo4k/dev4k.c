/* Development tool for the 4k demo (NOT part of the final executable).
 * Mirrors the GL usage of the assembly player (compat context, one RGBA32F
 * 1920x1080 target, fragment-only programs via glCreateShaderProgramv,
 * glRecti, time in uniform location 0) and its audio post-processing, and can
 * dump frames (PPM) and the soundtrack (WAV).
 *   dev4k [--min] --frame T out.ppm | --seq T0 T1 DT prefix | --wav out.wav | --bench
 * Shaders: common.glsl + img.glsl / snd.glsl from this directory, or with
 * --min the minified strings from build/demo4k_shaders.h style files.         */
#define DEV
#include "../common/sys.h"
#include "../common/gl.h"
#include "../common/plat_imports.h"
IMP(GLuint, glCreateShaderProgramv, (GLenum, GLsizei, const char *const *))
IMP(void, glUseProgram, (GLuint))
IMP(void, glUniform1f, (GLint, GLfloat))
IMP(GLint, glGetUniformLocation, (GLuint, const char *))
IMP(void, glRecti, (GLint, GLint, GLint, GLint))
IMP(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
IMP(void, glBindFramebufferEXT, (GLenum, GLuint))
IMP(void, glFramebufferTexture2DEXT, (GLenum, GLenum, GLenum, GLuint, GLint))
IMP(void, glGetProgramiv, (GLuint, GLenum, GLint *))
IMP(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, char *))
IMP_END
#include "../common/plat.c"
#include <math.h>

#define W 1920
#define H 1080
#define RATE 48000
#include "post.h"   /* audio post-processing reference (same algorithm as the asm) */

static char *load(const char *dir, const char *name) {
  char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *s = malloc(n + 2); fread(s, 1, n, f); s[n] = 0; fclose(f); return s;
}
static GLuint prog(const char *a, const char *b) {
  const char *src[2] = {a, b};
  GLuint p = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 2, src);
  GLint ok = 0; char log[8192] = ""; glGetProgramiv(p, GL_LINK_STATUS, &ok); glGetProgramInfoLog(p, sizeof log, 0, log);
  if (!ok || log[0]) fprintf(stderr, "---- program: %s\n%s\n", ok ? "warnings" : "ERROR", log);
  if (!ok) exit(1);
  return p;
}
static float *px;
static void ppm(const char *path) {
  static u8 *b; if (!b) b = malloc(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, b);
  FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fwrite(b + ((size_t)y * W + x) * 4, 1, 3, f);  /* row 0 = top (shader renders top-down) */
  fclose(f);
}
int main(int argc, char **argv) {
  const char *dir = DEMO_DIR; int min = 0;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--min")) min = 1;
  imp_load();
  if (!plat_egl_init()) { fprintf(stderr, "no GL\n"); return 1; }
  glBindTexture(GL_TEXTURE_2D, 1);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, SND_W, H);
  glBindFramebufferEXT(GL_FRAMEBUFFER, 1);   /* EXT entry points accept a user-chosen name in the compat profile */
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 1, 0);
  glViewport(0, 0, W, H);
  double c0 = plat_clock();
  char *com = load(dir, min ? "min/common.glsl" : "common.glsl");
  GLuint pi = prog(com, load(dir, min ? "min/img.glsl" : "img.glsl"));
  GLuint ps = prog(com, load(dir, min ? "min/snd.glsl" : "snd.glsl"));
  glFinish();
  fprintf(stderr, "compile: %.0f ms; uniform t location img=%d\n", (plat_clock() - c0) * 1e3, glGetUniformLocation(pi, "t"));
  px = malloc((size_t)SND_W * H * 4 * sizeof(float));
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--wav") && i + 1 < argc) {
      c0 = plat_clock();
      glViewport(0, 0, SND_W, SND_H);
      glUseProgram(ps); glRecti(-1, -1, 1, 1);
      glReadPixels(0, 0, SND_W, SND_H, GL_RGBA, GL_FLOAT, px);
      glViewport(0, 0, W, H);
      double c1 = plat_clock();
      post_process(px, SND_W * SND_H);
      fprintf(stderr, "sound render: %.0f ms, post: %.0f ms\n", (c1 - c0) * 1e3, (plat_clock() - c1) * 1e3);
      int n = (int)(SONG_SECONDS * RATE);
      { float pk = 0; for (int k = 0; k < n * 2; k++) { float a = fabsf(px[k]); if (a > pk) pk = a; }
        fprintf(stderr, "true peak %.3f (%.2f dBFS)\n", pk, 20 * log10f(pk));
        for (int sec = 0; sec < 8; sec++) { double e = 0; float p2 = 0; int a0 = sec * n / 8 * 2, a1 = (sec + 1) * n / 8 * 2; for (int k = a0; k < a1; k++) { e += px[k] * px[k]; if (fabsf(px[k]) > p2) p2 = fabsf(px[k]); }
          fprintf(stderr, "  %4.1f-%4.1fs rms %.1f dB peak %.2f\n", sec * 7.5, sec * 7.5 + 7.5, 10 * log10(e / (a1 - a0) + 1e-12), p2); } }
      FILE *f = fopen(argv[++i], "wb");
      int hdr[11] = {0x46464952, 36 + n * 4, 0x45564157, 0x20746d66, 16, 0x00020001, RATE, RATE * 4, 0x00100004, 0x61746164, n * 4};
      fwrite(hdr, 4, 11, f);
      for (int k = 0; k < n * 2; k++) { float v = px[k] * 32767.f; short s = v > 32767 ? 32767 : v < -32768 ? -32768 : (short)v; fwrite(&s, 2, 1, f); }
      fclose(f);
    } else if (!strcmp(argv[i], "--frame") && i + 2 < argc) {
      glUseProgram(pi); glUniform1f(0, atof(argv[i + 1])); glRecti(-1, -1, 1, 1); ppm(argv[i + 2]); i += 2;
    } else if (!strcmp(argv[i], "--seq") && i + 4 < argc) {
      double t0 = atof(argv[i + 1]), t1 = atof(argv[i + 2]), dt = atof(argv[i + 3]); char fn[1024];
      glUseProgram(pi);
      for (double t = t0; t <= t1 + 1e-9; t += dt) { snprintf(fn, sizeof fn, "%s_%06.2f.ppm", argv[i + 4], t); glUniform1f(0, t); glRecti(-1, -1, 1, 1); ppm(fn); }
      i += 4;
    } else if (!strcmp(argv[i], "--bench")) {
      glUseProgram(pi);
      for (double t = 0; t < 60; t += 5) {
        glUniform1f(0, t); glRecti(-1, -1, 1, 1); glFinish(); c0 = plat_clock();
        for (int k = 0; k < 20; k++) { glUniform1f(0, t + k / 60.0); glRecti(-1, -1, 1, 1); }
        glFinish(); fprintf(stderr, "t=%5.1f  %.2f ms/frame\n", t, (plat_clock() - c0) * 1e3 / 20);
      }
    }
  }
  return 0;
}
