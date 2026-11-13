#include "../common/platform.h"
#include "../../build/demo4k_shaders.h"

#define SAMPLE_RATE 48000

typedef struct {
    int   (*Init)(uint32_t);
    void *(*CreateWindow)(const char *, int, int, int, int, uint32_t);
    void *(*GL_CreateContext)(void *);
    void *(*GL_GetProcAddress)(const char *);
    int   (*GL_SetSwapInterval)(int);
    void  (*GL_GetDrawableSize)(void *, int *, int *);
    void  (*GL_SwapWindow)(void *);
    int   (*ShowCursor)(int);
    int   (*OpenAudio)(SDL_AudioSpec *, SDL_AudioSpec *);
    void  (*PauseAudio)(int);
    int   (*PollEvent)(void *);
    int   (*GetTicks)(void);
    void  (*Delay)(uint32_t);
    void  (*Quit)(void);
} SdlApi;

typedef struct {
    void     (*Viewport)(int, int, int, int);
    uint32_t (*CreateShaderProgramv)(uint32_t, int, const char **);
    void     (*UseProgram)(uint32_t);
    void     (*Uniform4fv)(int, int, const float *);
    void     (*Rects)(short, short, short, short);
} GlApi;

static const char sdl_names[] =
    "SDL_Init\0"
    "SDL_CreateWindow\0"
    "SDL_GL_CreateContext\0"
    "SDL_GL_GetProcAddress\0"
    "SDL_GL_SetSwapInterval\0"
    "SDL_GL_GetDrawableSize\0"
    "SDL_GL_SwapWindow\0"
    "SDL_ShowCursor\0"
    "SDL_OpenAudio\0"
    "SDL_PauseAudio\0"
    "SDL_PollEvent\0"
    "SDL_GetTicks\0"
    "SDL_Delay\0"
    "SDL_Quit\0";

static const char gl_names[] =
    "glViewport\0"
    "glCreateShaderProgramv\0"
    "glUseProgram\0"
    "glUniform4fv\0"
    "glRects\0";

static const uint8_t chords[8][4] = {
    {7, 10, 14, 21}, {3, 7, 10, 14}, {0, 3, 7, 14}, {2, 7, 9, 12},
    {7, 11, 14, 21}, {4, 7, 11, 18}, {0, 4, 11, 18}, {2, 7, 9, 16}
};

typedef struct {
    int sp, rng;
    int env[4];
    int flt[3];
    int ph[8];
    float u[8];
    SdlApi sdl;
    GlApi gl;
    uint32_t ev[16];
    int inc[64];
    int16_t sin_tab[4096];
    int16_t dl_buf[2][16384];
} SynthState;

static SynthState S;

#define SIN16(s, p) ((int)(s)->sin_tab[((uint32_t)(p)) >> 20])

static void audio_cb(void *ud, uint8_t *stream, int len) {
    SynthState *s = (SynthState *)ud;
    int16_t *out = (int16_t *)stream;
    int frames = len >> 2;

    for (int i = 0; i < frames; i++) {
        int sp = s->sp++;
        int step = sp / 6000, spos = sp % 6000;
        int bar = step >> 4, s16 = step & 15;
        int is_res = (bar >= 21);
        const uint8_t *ch = chords[((bar >> 1) & 3) + (is_res ? 4 : 0)];

        int wn = (s->rng = s->rng * 1103515245 + 12345) >> 17;

        int nA = (bar < 9) ? (9 - bar) : 0;
        int oA = (bar > 8) ? 8 : bar;
        int cA = (bar > 20) ? 4 : ((bar > 18) ? 2 : 0);

        if (spos == 0) {
            if (bar >= 4 && bar < 27 && (((bar < 9 ? 0x0401 : (is_res ? 0x1111 : 0x5111)) >> s16) & 1)) {
                s->env[0] = 16000; s->ph[0] = 0;
            }
            if (bar >= 9 && bar < 26) {
                if (((bar == 20 ? 0x5555 : 0x1010) >> s16) & 1) s->env[1] = 13000;
                else if (bar >= 11 && s->env[1] < 2400) s->env[1] = (s16 & 1) ? 3400 : 1900;
            }
            if (bar >= 5 && bar < 29 && (bar >= 12 || !(s16 & 3))) s->env[3] = 16000;
        }
        if (bar < 9 && !(sp & 511) && wn > 11500) s->env[1] = nA << 9;

        int ris = (bar == 19 || bar == 20);
        s->flt[2] += ((wn - s->flt[2]) * (ris ? 55 : 14)) >> 8;
        int hp = wn - s->flt[2];
        int sl = (hp * (nA + (ris ? 12 : 0))) >> 6, sr = sl;

        int ke = s->env[0], se = s->env[1], ae = s->env[3];
        s->ph[0] += 3937050 + ((((ke * ke) >> 14) * ke) >> 14) * 750;
        int drm = ((SIN16(s, s->ph[0]) * ke) >> 14) + ((hp * ((se * se) >> 14)) >> 15);
        sl += drm; sr += drm;
        s->env[0] = ke - (ke >> 11);
        s->env[1] = se - (se >> 9);

        int bi = s->inc[ch[0] + (s16 == 14 ? 12 : 0)];
        int saw = ((s->ph[1] += bi - (bi >> 8)) >> 18) + ((s->ph[2] += bi + (bi >> 8)) >> 18);
        int bp = 16384 - spos * 2;
        s->env[2] = (bp * oA) >> 3;
        int cut = 8 + ((bp * oA) >> 12);
        s->flt[0] += (cut * s->flt[1]) >> 8;
        s->flt[1] += (cut * (saw - s->flt[0] - (s->flt[1] >> 2))) >> 8;
        sl += s->flt[0] >> 1;
        sr += s->flt[0] >> 1;

        s->ph[3] += s->inc[ch[(s16 ^ (s16 >> 2)) & 3] + 24 + ((s16 & 4) ? 12 : 0)];
        int bell = (SIN16(s, s->ph[3] + ((SIN16(s, s->ph[3] << 1) * ((ae * ae) >> 14)) << 1)) * ae) >> 16;
        sl += (step & 1) ? bell : (bell >> 1);
        sr += (step & 1) ? (bell >> 1) : bell;
        s->env[3] = ae - (ae >> 11);

        #pragma GCC unroll 0
        for (int v = 0; v < 4; v++) {
            int pi = s->inc[ch[v] + 12];
            int p = (s->ph[4 + v] += pi + (v - 1) * (pi >> 9));
            int osc = ((SIN16(s, p) * 2 + (p >> 17)) * cA) >> 7;
            sl += osc;
            sr += (v & 1) ? (osc >> 1) : osc;
        }

        int di = sp & 16383, lr[2] = {sl, sr}, d0 = s->dl_buf[0][di], d1 = s->dl_buf[1][di];
        int dl[2] = {d0, d1};
        s->dl_buf[0][di] = (int16_t)((sl + d1) * 3 >> 3);
        s->dl_buf[1][di] = (int16_t)((sr + d0) * 3 >> 3);

        int a = sp >> 9, b = (2880000 - sp) >> 9;
        int fd = a < b ? a : b;
        fd = fd > 256 ? 256 : (fd < 0 ? 0 : fd);

        #pragma GCC unroll 0
        for (int c = 0; c < 2; c++) {
            int m = ((lr[c] + (dl[c] >> 1)) * fd) >> 9;
            *out++ = (int16_t)(m > 32000 ? 32000 : (m < -32000 ? -32000 : m));
        }
    }
}

static SDL_AudioSpec want_spec = {
    SAMPLE_RATE, AUDIO_S16LSB, 2, 0, 512, 0, 0, audio_cb, &S
};

void demo4k_main(void) {
    SynthState *s = &S;
    s->rng = 1;

    int c = 16384 << 10, sn = 0;
    for (int i = 0; i < 4096; i++) {
        s->sin_tab[i] = (int16_t)(sn >> 10);
        c -= (sn * 201) >> 17;
        sn += (c * 201) >> 17;
    }
    int f = 4384446;
    for (int n = 0; n < 64; n++) {
        s->inc[n] = f;
        f = (int)((int64_t)f * 69433 >> 16);
    }

    load_dlsyms(dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_GLOBAL), sdl_names, (void **)&s->sdl);

    s->sdl.Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    void *win = s->sdl.CreateWindow("", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESK);
    s->sdl.GL_CreateContext(win);
    s->sdl.GL_SetSwapInterval(1);
    s->sdl.ShowCursor(0);
    load_glsyms(s->sdl.GL_GetProcAddress, gl_names, (void **)&s->gl);

    const char *src = SHADER_SCENE_4K;
    s->gl.UseProgram(s->gl.CreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &src));

    s->sdl.OpenAudio(&want_spec, 0);
    s->sdl.PauseAudio(0);

    int t0 = s->sdl.GetTicks();
    for (;;) {
        int ft0 = s->sdl.GetTicks();
        while (s->sdl.PollEvent(s->ev)) {
            if (s->ev[0] == SDL_QUIT_EV || (s->ev[0] == SDL_KEYDOWN_EV && s->ev[4] == SDL_SCANCODE_ESC)) {
                goto done;
            }
        }

        float t = (float)(ft0 - t0) * 0.001f;
        if (t >= 60.0f) break;

        int dw, dh;
        s->sdl.GL_GetDrawableSize(win, &dw, &dh);
        s->gl.Viewport(0, 0, dw, dh);

        s->u[0] = (float)dw;
        s->u[1] = (float)dh;
        s->u[2] = t;
        for (int i = 0; i < 4; i++) s->u[4 + i] = (float)s->env[i];

        s->gl.Uniform4fv(0, 2, s->u);
        s->gl.Rects(-1, -1, 1, 1);
        s->sdl.GL_SwapWindow(win);

        if (s->sdl.GetTicks() - ft0 < 4) s->sdl.Delay(15u);
    }

done:
    s->sdl.Quit();
    sys_exit_zero();
}

DEMO_ENTRY(demo4k_main)
