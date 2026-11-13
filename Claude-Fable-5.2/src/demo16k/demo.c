/* MorphDemo 16k - "CULTURE": a reaction-diffusion culture grows from noise to order.
 *
 * A Gray-Scott simulation (960x540, ping-pong float textures, 3600 steps per second of
 * demo time, so its state is independent of the frame rate) is rendered as a wet,
 * glowing relief: ray-marched height field, soft shadows, reflections, depth of field.
 * The soundtrack is a GLSL "sound shader" rendered once at start-up (plus delay,
 * reverb and limiter on the CPU); image, simulation and sound share one score chunk
 * (score.glsl), which is what keeps them in sync.
 *
 *   96 BPM, bar = 2.5 s
 *   bars  0- 5   0-15 s  EMERGENCE   grain and sparks that die; droplet notes (random time
 *                                    and pitch) seed rings; colonies of worms spread
 *   bars  6-15  15-40 s  TRANSFORM   beat enters; labyrinth -> honeycomb -> stripes that
 *                                    comb themselves parallel; camera dives into the relief;
 *                                    every kick sends a magenta ring through the tissue
 *   bars 16-19  40-50 s  ORDER       the drop: stripes shatter into a lattice of pearls
 *                                    (D minor resolves to D major, lead line lights the lattice)
 *   bars 20-23  50-59 s  RESOLUTION  everything but the word starves: MORPH, written in
 *                                    pearls; caption; final chord; fade to black
 */
#include "../common/sys.h"
#include "../common/gl.h"
#include "../common/plat_imports.h"
#include "../common/eng_imports.h"
IMP_END
#include "../common/plat.c"
#define DEMO_LEN 59.5
#define NU 8
#include "../common/eng.c"
#include "../common/fx.c"

#if defined(DEV) && !defined(USE_GEN)
#define CH(id, file) static const char id[] = file;
#include "chunks.def"
#else
#include "shaders_gen.h"
#endif
static const char *const vs_quad[] = {c_hdr, c_vs, 0};
static const char *const fs_sim[] = {c_hdr, c_score, c_sim, 0};
static const char *const fs_main[] = {c_hdr, c_score, c_main, 0};
static const char *const fs_post[] = {c_hdr, c_score, c_post, 0};
static const char *const fs_snd[] = {c_hdr, c_score, c_snd, 0};

#define SIMW 960
#define SIMH 540
#define SPS 3600
#define SS 2 /* supersampling factor of the relief pass */
static GLuint p_sim, p_main, p_post, p_snd;
static RT rt_sim[2], rt_hdr, rt_out;
static int sim_cur, sim_steps;
static float st[4][AU_LEN_FRAMES * 2];

/* ---- keyframe tables ----
 * simulation: time, feed, kill, dt, noise, seed rate, kill variation, text wipe, kick wave   (linear)
 * camera:     time, target x, target y, distance, pitch, yaw, tan(fov/2), focus(rel), aperture   (Catmull-Rom) */
#define KC 12
#define CC 9
#define LC 9
#ifdef DEV
#define KDIM(c) 64 * (c)   /* room for tables given on the command line (RDS / RDC / RDL) */
#else
#define KDIM(c)
#endif
static float keys[KDIM(KC)] = {
  /* t     feed     kill     dt    noise    seeds    kvar     wipe kick     aniso drift  angle */
  0,     0.0367f, 0.072f,  0.3f, 0.0015f, 0.012f,  0,       0,   0,       0,    0,      0.6f, /* sparks that cannot survive */
  1.6f,  0.0367f, 0.072f,  0.3f, 0.0015f, 0.012f,  0,       0,   0,       0,    0,      0.6f,
  1.9f,  0.0367f, 0.0662f, 0.2f, 0.0015f, 0.0015f, 0,       0,   0,       0,    0,      0.6f, /* first rings, slow growth    */
  14,    0.0367f, 0.0662f, 0.3f, 0.0015f, 0.002f,  0,       0,   0,       0,    0,      0.6f,
  15,    0.0367f, 0.066f,  1,    0.0012f, 0,       0.0005f, 0,   0.0008f, 0,    0,      0.6f, /* colonies merge              */
  20,    0.03f,   0.057f,  1,    0,       0,       0.001f,  0,   0.0008f, 0,    0,      0.6f, /* labyrinth                   */
  22.5f, 0.039f,  0.058f,  1,    0,       0,       0.0005f, 0,   0.0008f, 0,    0,      0.6f, /* cells: honeycomb patches    */
  27.5f, 0.039f,  0.058f,  1,    0,       0,       0.0005f, 0,   0.0008f, 0,    0,      0.6f,
  30,    0.03f,   0.057f,  1,    0,       0,       0,       0,   0.0008f, 0.5f, 0.017f, 0.6f, /* stripes comb themselves parallel */
  37.5f, 0.03f,   0.057f,  1,    0,       0,       0,       0,   0.0008f, 0.6f, 0.017f, 0.6f,
  38.9f, 0.03f,   0.057f,  1,    0,       0,       0,       0,   0.0008f, 0.6f, 0.017f, 0.6f,
  40.1f, 0.0367f, 0.0649f, 1,    0,       0,       0,       0,   0.0008f, 0,    0,      0.6f, /* stripes shatter into a lattice of pearls */
  50,    0.0367f, 0.0649f, 1,    0,       0,       0,       0,   0,       0,    0,      0.6f,
  51.5f, 0.0367f, 0.0649f, 1,    0,       0,       0,       11,  0,       0,    0,      0.6f, /* everything but the word starves */
  60,    0.0367f, 0.0649f, 1,    0,       0,       0,       11,  0,       0,    0,      0.6f,
};
static int nkeys = 15;
static float ckeys[KDIM(CC)] = {
  /* t     tx    ty    dist   pitch    yaw       tanfov  focus aperture */
  0,     8,    4.5f, 7,     1.25f,   -1.2f,    0.225f, 1,    60,
  8,     8.2f, 4.6f, 7.5f,  1.2f,    -1.35f,   0.225f, 1,    60,
  15,    8.5f, 4.7f, 8,     1.1f,    -1.5f,    0.225f, 1,    50,
  22,    9,    5,    6.5f,  0.62f,   -1.75f,   0.3f,   0.9f, 30,
  30,    9.5f, 5.5f, 6.5f,  0.55f,   -2.1f,    0.3f,   0.9f, 30,
  37.5f, 9,    5,    6,     0.42f,   -2.54f,   0.32f,  0.9f, 30,
  42,    8.5f, 4.8f, 9,     0.9f,    -2.1f,    0.28f,  1,    30,
  47,    8,    4.5f, 13,    1.3f,    -1.7f,    0.25f,  1,    15,
  50,    8,    4.5f, 20,    1.5708f, -1.5708f, 0.225f, 1,    0,
  60,    8,    4.5f, 18.4f, 1.46f,   -1.5708f, 0.225f, 1,    10,
};
static int nckeys = 10;
static float lkeys[KDIM(LC)] = {
  /* t      fade  bloom grain  boil   glow  accent light  caption */
  0,      0,    1,    0.12f, 1.8f,  0.8f, 1,     0, 0,
  1.2f,   1,    1,    0.1f,  1.5f,  0.8f, 1,     0.05f, 0,
  6,      1,    1,    0.05f, 0.3f,  0.8f, 1,     0.2f, 0,
  14.98f, 1,    1,    0.03f, 0.08f, 0.8f, 1,     0.5f, 0,
  15.04f, 1.4f, 1.8f, 0.03f, 0.08f, 1.1f, 1,     0.5f, 0,   /* the beat enters: flash, colour blooms */
  15.9f,  1,    1,    0.03f, 0.08f, 1,    1,     0.52f, 0,
  37.5f,  1,    1,    0.03f, 0.02f, 1,    1,     1.15f, 0,
  39.98f, 1.15f,1.6f, 0.03f, 0.02f, 1.7f, 1.6f,  1.2f, 0,   /* build-up with the riser */
  40.04f, 1.5f, 2,    0.03f, 0.02f, 1.3f, 1,     1.2f, 0,   /* flash on the drop */
  41,     1,    1,    0.03f, 0.02f, 1.3f, 1,     1.25f, 0,
  49.98f, 1,    1,    0.025f,0.01f, 1.4f, 1,     1.6f, 0,
  50.04f, 1.6f, 2,    0.025f,0.01f, 1.6f, 1,     1.6f, 0,   /* flash on the final hit */
  51,     1,    1.2f, 0.025f,0,     1.6f, 1,     1.65f, 0,
  52.8f,  1,    1.2f, 0.025f,0,     1.6f, 1,     1.7f, 0,
  54.2f,  1,    1.2f, 0.025f,0,     1.6f, 1,     1.75f, 0.8f,
  57.3f,  1,    1.2f, 0.025f,0,     1.6f, 1,     1.9f, 0.8f,
  59.3f,  0,    1.2f, 0.025f,0,     1.6f, 1,     2, 0.8f,
  60,     0,    1.2f, 0.025f,0,     1.6f, 1,     2, 0.8f,
};
static int nlkeys = 18;

#ifdef DEV
static int dbg_parse(const char *e, float *tab, int cols) {
  int n = 0;
  while (*e) {
    float *k = tab + n * cols; int i;
    for (i = 0; i < cols; i++) k[i] = n ? k[i - cols] : 0;
    i = 0;
    while (*e && *e != ';') { k[i++] = strtof(e, (char **)&e); if (*e == ':') e++; }
    if (*e == ';') e++;
    n++;
  }
  return n;
}
static void dbg_env(void) {
  if (getenv("RDS")) nkeys = dbg_parse(getenv("RDS"), keys, KC);
  if (getenv("RDC")) nckeys = dbg_parse(getenv("RDC"), ckeys, CC);
  if (getenv("RDL")) nlkeys = dbg_parse(getenv("RDL"), lkeys, LC);
}
#endif

static void lerpkeys(const float *k, int n, int cols, float t, float *o) {
  int i = 0, j;
  float a;
  while (i < n - 2 && t >= k[(i + 1) * cols]) i++;
  a = (t - k[i * cols]) / (k[(i + 1) * cols] - k[i * cols]);
  a = a < 0 ? 0 : a > 1 ? 1 : a;
  for (j = 1; j < cols; j++) o[j - 1] = k[i * cols + j] + (k[(i + 1) * cols + j] - k[i * cols + j]) * a;
}

static void cam(float t, float *o) {
  int i = 0, j;
  while (i < nckeys - 2 && t >= ckeys[(i + 1) * CC]) i++;
  {
    const float *p1 = ckeys + i * CC, *p2 = p1 + CC, *p0 = i ? p1 - CC : p1, *p3 = i < nckeys - 2 ? p2 + CC : p2;
    float d = p2[0] - p1[0], a = (t - p1[0]) / d, a2, a3;
    a = a < 0 ? 0 : a > 1 ? 1 : a; a2 = a * a; a3 = a2 * a;
    for (j = 1; j < CC; j++) {
      float m1 = (p2[j] - p0[j]) * d / (p2[0] - p0[0]), m2 = (p3[j] - p1[j]) * d / (p3[0] - p1[0]);
      if ((p2[j] - p1[j]) * (p1[j] - p0[j]) <= 0) m1 = 0; /* no overshoot at holds / turning points */
      if ((p3[j] - p2[j]) * (p2[j] - p1[j]) <= 0) m2 = 0;
      o[j - 1] = (2 * a3 - 3 * a2 + 1) * p1[j] + (a3 - 2 * a2 + a) * m1 + (-2 * a3 + 3 * a2) * p2[j] + (a3 - a2) * m2;
    }
  }
}

static void demo_init(void) {
#ifdef DEV
  dbg_env();
#endif
  p_sim = eng_prog(vs_quad, fs_sim, 0);
  p_main = eng_prog(vs_quad, fs_main, 0);
  p_post = eng_prog(vs_quad, fs_post, 0);
  p_snd = eng_prog(vs_quad, fs_snd, 0);
  rt_make(&rt_sim[0], SIMW, SIMH, GL_RG32F, 1, 5, GL_LINEAR, GL_REPEAT);
  rt_make(&rt_sim[1], SIMW, SIMH, GL_RG32F, 1, 5, GL_LINEAR, GL_REPEAT);
  rt_make(&rt_hdr, RW * SS, RH * SS, GL_RGBA16F, 1, 9, GL_LINEAR, GL_CLAMP_TO_EDGE);
  rt_make(&rt_out, RW, RH, GL_RGBA8, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE);
}

/* The simulation advances by demo time (SPS steps per second), so its state at a
 * given moment does not depend on the frame rate.                              */
static void sim_advance(double t) {
  int target = (int)(t * SPS) + 1, n;
#ifdef DEV
  if (target < sim_steps) sim_steps = 0;            /* dev tool may jump back in time: replay from the start */
#endif
  n = target - sim_steps;
#ifndef DEV
  if (n > 240) n = 240;                             /* bounded catch-up after a hiccup */
#endif
  while (n-- > 0) {
    eng_u[4] = (float)sim_steps;
    { float k[KC]; int j; lerpkeys(keys, nkeys, KC, (float)sim_steps / SPS, k);
      for (j = 0; j < 8; j++) eng_u[8 + j] = k[j];
      eng_u[5] = k[8]; eng_u[6] = k[9]; eng_u[7] = k[10]; }
    cam((float)sim_steps / SPS, eng_u + 16);
    rt_bind(&rt_sim[1 - sim_cur]);
    eng_tex(0, rt_sim[sim_cur].tex[0]);
    eng_use(p_sim);
    eng_tri();
    sim_cur ^= 1;
    sim_steps++;
  }
}

static GLuint demo_frame(double t) {
  float c[CC];
  sim_advance(t);
  glBindTexture(GL_TEXTURE_2D, rt_sim[sim_cur].tex[0]);
  glGenerateMipmap(GL_TEXTURE_2D);
  cam((float)t, c);
  eng_u[16] = c[0]; eng_u[17] = c[1]; eng_u[18] = c[2]; eng_u[19] = c[3];
  eng_u[20] = c[4]; eng_u[21] = c[5]; eng_u[22] = c[2] * c[6]; eng_u[23] = c[7];
  lerpkeys(lkeys, nlkeys, LC, (float)t, eng_u + 24);
  rt_bind(&rt_hdr);
  eng_u[1] = RW * SS; eng_u[2] = RH * SS;
  eng_tex(1, rt_sim[1 - sim_cur].tex[0]);
  eng_tex(0, rt_sim[sim_cur].tex[0]);
  eng_use(p_main);
  eng_tri();
  glBindTexture(GL_TEXTURE_2D, rt_hdr.tex[0]);
  glGenerateMipmap(GL_TEXTURE_2D);
  rt_bind(&rt_out);
  eng_u[1] = RW; eng_u[2] = RH;
  eng_use(p_post);
  eng_tri();
  return rt_out.fbo;
}

static void demo_audio(float *buf, int n) {
  float *stems[4] = {st[0], st[1], st[2], st[3]};
  int i;
  eng_sound(p_snd, 2, stems, n);
  for (i = 0; i < 2 * n; i++) buf[i] = st[0][i];
  fx_delay(st[2], buf, n, 22500, 0.45f, 0.5f);        /* dotted-eighth ping-pong on the droplets */
  for (i = 0; i < 2 * n; i++) st[1][i] += 0.25f * (buf[i] - st[0][i]);
  fx_reverb(st[1], buf, n, 0.86f, 0.3f, 0.5f);
  fx_master(buf, n, 0.72f);
  for (i = 0; i < 2 * n; i++) {                       /* true peak below -1 dBFS; end in silence */
    float e = (59.4f * AU_RATE - (i >> 1)) / (0.5f * AU_RATE);
    buf[i] *= 0.84f * (e < 0 ? 0 : e > 1 ? 1 : e);
  }
}
