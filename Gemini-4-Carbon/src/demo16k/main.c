#include "../common/platform.h"
#include "../../build/demo16k_shaders.h"

#define SAMPLE_RATE 48000
#define DELAY_LEN   32768
#define DELAY_MASK  (DELAY_LEN - 1)

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
} SdlApi16;

typedef struct {
    void     (*Viewport)(int, int, int, int);
    uint32_t (*CreateShaderProgramv)(uint32_t, int, const char **);
    void     (*UseProgram)(uint32_t);
    void     (*Uniform4fv)(int, int, const float *);
    void     (*Rects)(short, short, short, short);
    void     (*GenTextures)(int, uint32_t *);
    void     (*ActiveTexture)(uint32_t);
    void     (*BindTexture)(uint32_t, uint32_t);
    void     (*TexImage2D)(uint32_t, int, int, int, int, int, uint32_t, uint32_t, const void *);
    void     (*TexParameteri)(uint32_t, uint32_t, int);
    void     (*GenFramebuffers)(int, uint32_t *);
    void     (*BindFramebuffer)(uint32_t, uint32_t);
    void     (*FramebufferTexture2D)(uint32_t, uint32_t, uint32_t, uint32_t, int);
} GlApi16;

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
    "glRects\0"
    "glGenTextures\0"
    "glActiveTexture\0"
    "glBindTexture\0"
    "glTexImage2D\0"
    "glTexParameteri\0"
    "glGenFramebuffers\0"
    "glBindFramebuffer\0"
    "glFramebufferTexture2D\0";

/* MIDI note offsets relative to D2 (MIDI 38, ~73.42 Hz)
 * 0..3: D-Aeolian/Dorian abyssal progression (Dm9, BbMaj7#11, Gm9, Asus4/C)
 * 4..7: F-Lydian luminous sanctuary progression (Fmaj9#11, G6/F, Em9, Am11)
 */
static const uint8_t chords16[8][5] = {
    { 0,  7, 10, 14, 17},
    { 8, 14, 19, 21, 24},
    { 5, 12, 15, 19, 22},
    {10, 14, 19, 21, 26},
    { 3, 10, 14, 19, 23},
    { 3, 12, 17, 21, 26},
    { 2,  9, 14, 17, 23},
    { 7, 14, 19, 22, 26}
};

/* Polyrhythmic 3-against-4 voice index pattern for glass marimba */
static const uint8_t marimba_pat[16] = {
    0, 2, 4, 1, 3, 4, 2, 0, 4, 1, 3, 2, 4, 3, 1, 0
};

typedef struct {
    uint32_t sp;
    uint32_t rng;

    /* Envelopes & reactive levels */
    float sub_env;
    float drop_env;
    float brush_env;
    float bass_env;
    float pad_lvl;

    /* Oscillator phases */
    float sub_ph;
    float drop_ph;
    float drop_mod_ph;
    float drop_freq;
    float drop_ratio;
    float bass_ph[2];
    float bass_freq;
    float pad_ph[5];

    /* Filters: 4-pole ladder for bass, stereo SVF for hydrophone & pad */
    float lad[4];
    float hydro_bp[2], hydro_lp[2];
    float shaker_lp;
    float pad_flt[2];

    /* Modulated stereo aquatic delay */
    float dl_l[DELAY_LEN];
    float dl_r[DELAY_LEN];

    /* Audio-visual sync & GL/SDL state */
    float vis_env[4];
    float u[12];
    SdlApi16 sdl;
    GlApi16 gl;
    uint32_t ev[16];
    float note_hz[64];
} Demo16State;

static Demo16State G;

static inline float fast_wrap(float x) {
    return x - (float)(int)x;
}

static inline float soft_clip(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static void audio_cb16(void *ud, uint8_t *stream, int len) {
    Demo16State *s = (Demo16State *)ud;
    float *out = (float *)stream;
    int frames = len >> 3; /* stereo float32 = 8 bytes/frame */

    for (int i = 0; i < frames; i++) {
        uint32_t sp = s->sp++;
        /* 108 BPM: 16th note = 6667 samples (~0.1389s), 27 bars = 60.0s */
        uint32_t step = sp / 6667u;
        uint32_t spos = sp % 6667u;
        uint32_t bar  = step >> 4;
        uint32_t s16  = step & 15u;
        float t_sec   = (float)sp * (1.0f / 48000.0f);

        int is_lydian = (bar >= 14u);
        const uint8_t *ch = chords16[((bar >> 1) & 3u) + (is_lydian ? 4u : 0u)];

        /* White noise generator */
        uint32_t r = s->rng * 1664525u + 1013904223u;
        s->rng = r;
        float wn = ((float)(int32_t)r) * (1.0f / 2147483648.0f);

        /* Step triggers */
        if (spos == 0u) {
            /* Deep heartbeat sub-pulse */
            uint16_t kpat = (bar < 3u) ? 0x0001u : (bar < 7u ? 0x0101u : (bar < 25u ? 0x1105u : 0x0001u));
            if ((kpat >> s16) & 1u) {
                s->sub_env = 1.0f;
                s->sub_ph  = 0.0f;
            }

            /* Brushed shaker groove (Phases II-IV) */
            if (bar >= 7u && bar < 25u) {
                if (((0x5454u >> s16) & 1u) || (bar >= 13u && (s16 & 1u))) {
                    s->brush_env = ((s16 & 3u) == 2u) ? 0.85f : 0.36f;
                }
            }

            /* Bass target pitch & envelope */
            if (bar >= 5u && bar < 26u && ((0x9999u >> s16) & 1u)) {
                uint8_t bnote = ch[0] + ((s16 == 12u || s16 == 15u) ? 7u : 0u);
                s->bass_freq = s->note_hz[bnote];
                s->bass_env  = 0.95f;
            }

            /* Sonar droplets (Phase I) -> Polyrhythmic Glass Marimba (Phases II-IV) */
            int trig_drop = 0;
            if (bar < 7u) {
                trig_drop = (s16 == 0u || s16 == 7u || (bar >= 3u && s16 == 11u));
                if (trig_drop) {
                    uint8_t n = ch[marimba_pat[s16]] + 24u;
                    s->drop_freq  = s->note_hz[n];
                    s->drop_ratio = 3.48f - 1.48f * ((float)bar * 0.14f); /* inharmonic sonar -> pure glass */
                    s->drop_env   = 0.9f;
                }
            } else if (bar < 26u) {
                /* 3-against-4 crystalline glass pattern */
                trig_drop = ((s16 % 3u) == 0u) || (bar >= 14u && (s16 & 1u) == 0u);
                if (trig_drop) {
                    uint8_t v = marimba_pat[(step * 3u) & 15u];
                    uint8_t oct = ((step % 5u) == 0u) ? 24u : 12u;
                    s->drop_freq  = s->note_hz[ch[v] + oct];
                    s->drop_ratio = is_lydian ? 2.0f : 3.0f;
                    s->drop_env   = ((s16 % 3u) == 0u) ? 0.85f : 0.52f;
                }
            }
        }

        /* 1. Pelagic Hydrophone Stereo Current */
        float drift_amt = (t_sec < 18.0f) ? (1.0f - 0.65f * (t_sec / 18.0f)) : 0.28f;
        /* Swell transition wave around 42s (Sanctuary opening) */
        float swell = expf(-0.35f * (t_sec - 42.5f) * (t_sec - 42.5f));
        float f_cut = 0.018f + 0.012f * sinf(t_sec * 0.43f) + 0.045f * swell;
        s->hydro_lp[0] += f_cut * s->hydro_bp[0];
        s->hydro_bp[0] += f_cut * (wn * 0.35f - s->hydro_lp[0] - 0.25f * s->hydro_bp[0]);
        s->hydro_lp[1] += (f_cut * 1.18f) * s->hydro_bp[1];
        s->hydro_bp[1] += (f_cut * 1.18f) * (-wn * 0.35f - s->hydro_lp[1] - 0.25f * s->hydro_bp[1]);

        float mix_l = s->hydro_bp[0] * (0.28f * drift_amt + 0.32f * swell);
        float mix_r = s->hydro_bp[1] * (0.28f * drift_amt + 0.32f * swell);

        /* 2. Sub-Pulse & Brushed Shaker */
        float se = s->sub_env;
        float sub_hz = 46.0f + 95.0f * se * se * se;
        s->sub_ph = fast_wrap(s->sub_ph + sub_hz * (1.0f / 48000.0f));
        float sub_sig = sinf(s->sub_ph * 6.2831853f) * se * 0.42f;
        mix_l += sub_sig;
        mix_r += sub_sig;
        s->sub_env = se * 0.99945f;

        s->shaker_lp += 0.32f * (wn - s->shaker_lp);
        float hp_wn = wn - s->shaker_lp;
        float br = hp_wn * s->brush_env * s->brush_env * 0.16f;
        mix_l += br * 1.15f;
        mix_r += br * 0.85f;
        s->brush_env *= 0.9978f;

        /* 3. Warm 4-Pole Resonant Ladder Fretless Bass */
        if (s->bass_freq < 30.0f) s->bass_freq = s->note_hz[0];
        float bf = s->bass_freq;
        s->bass_ph[0] = fast_wrap(s->bass_ph[0] + bf * (0.997f / 48000.0f));
        s->bass_ph[1] = fast_wrap(s->bass_ph[1] + bf * (1.003f / 48000.0f));
        float bsaw = ((s->bass_ph[0] * 2.0f - 1.0f) + (s->bass_ph[1] * 2.0f - 1.0f)) * 0.5f;
        float bcut = 0.035f + 0.13f * s->bass_env;
        float fb_in = bsaw - 2.6f * soft_clip(s->lad[3]);
        for (int p = 0; p < 4; p++) {
            float inp = (p == 0) ? fb_in : s->lad[p - 1];
            s->lad[p] += bcut * (soft_clip(inp) - s->lad[p]);
        }
        float bass_sig = s->lad[3] * (0.12f + 0.34f * s->bass_env) * (bar >= 5u ? 1.0f : 0.0f);
        mix_l += bass_sig;
        mix_r += bass_sig;
        s->bass_env *= 0.99962f;

        /* 4. FM Sonar Droplet / Glass Marimba */
        float de = s->drop_env;
        if (de > 0.0005f) {
            float df = s->drop_freq;
            s->drop_ph     = fast_wrap(s->drop_ph + df * (1.0f / 48000.0f));
            s->drop_mod_ph = fast_wrap(s->drop_mod_ph + df * s->drop_ratio * (1.0f / 48000.0f));
            float mod_idx  = de * de * (bar < 7u ? 2.6f : 1.35f);
            float fm = sinf((s->drop_ph + 0.22f * mod_idx * sinf(s->drop_mod_ph * 6.2831853f)) * 6.2831853f);
            float sig = fm * de * 0.21f;
            float pan = 0.5f + 0.35f * sinf((float)step * 1.7f);
            mix_l += sig * pan;
            mix_r += sig * (1.0f - pan);
            s->drop_env = de * (bar < 7u ? 0.99972f : 0.99952f);
        }

        /* 5. 5-Voice Lush Detuned Sea-Pad */
        float target_pad = (t_sec < 12.0f) ? 0.0f : ((t_sec < 30.0f) ? 0.35f : 0.85f);
        s->pad_lvl += 0.00008f * (target_pad - s->pad_lvl);
        if (s->pad_lvl > 0.002f) {
            float pl = 0.0f, pr = 0.0f;
            for (int v = 0; v < 5; v++) {
                float pf = s->note_hz[ch[v] + 12u] * (1.0f + (float)(v - 2) * 0.0022f);
                float ph = fast_wrap(s->pad_ph[v] + pf * (1.0f / 48000.0f));
                s->pad_ph[v] = ph;
                float tri = 1.0f - 4.0f * fabsf(ph - 0.5f);
                float osc = 0.65f * tri + 0.35f * (ph * 2.0f - 1.0f);
                pl += osc * ((v & 1) ? 0.45f : 0.85f);
                pr += osc * ((v & 1) ? 0.85f : 0.45f);
            }
            float pcut = 0.055f + 0.035f * sinf(t_sec * 0.31f) + (is_lydian ? 0.045f : 0.0f);
            s->pad_flt[0] += pcut * (pl * 0.065f * s->pad_lvl - s->pad_flt[0]);
            s->pad_flt[1] += pcut * (pr * 0.065f * s->pad_lvl - s->pad_flt[1]);
            mix_l += s->pad_flt[0];
            mix_r += s->pad_flt[1];
        }

        /* 6. Modulated Stereo Chorus / Ping-Pong Delay (dotted 8th = 20000 samples) */
        uint32_t widx = sp & DELAY_MASK;
        float mod_ofs = 28.0f * sinf(t_sec * 1.9f);
        uint32_t ridx_l = (sp - 20000u + (uint32_t)(int)(32.0f + mod_ofs)) & DELAY_MASK;
        uint32_t ridx_r = (sp - 13334u + (uint32_t)(int)(32.0f - mod_ofs)) & DELAY_MASK;
        float tap_l = s->dl_l[ridx_l];
        float tap_r = s->dl_r[ridx_r];

        s->dl_l[widx] = soft_clip(mix_l * 0.52f + tap_r * 0.54f);
        s->dl_r[widx] = soft_clip(mix_r * 0.52f + tap_l * 0.54f);

        float out_l = soft_clip((mix_l + tap_l * 0.44f) * 0.92f);
        float out_r = soft_clip((mix_r + tap_r * 0.44f) * 0.92f);

        /* Master fade-in (0..1.8s) and fade-out (57.5..60.0s) */
        float fade_in  = t_sec * 0.55f;
        float fade_out = (60.0f - t_sec) * 0.4f;
        float gain = fade_in < fade_out ? fade_in : fade_out;
        if (gain > 1.0f) gain = 1.0f;
        if (gain < 0.0f) gain = 0.0f;

        out[i * 2 + 0] = out_l * gain;
        out[i * 2 + 1] = out_r * gain;
    }

    s->vis_env[0] = s->sub_env;
    s->vis_env[1] = s->drop_env;
    s->vis_env[2] = s->brush_env + 0.25f;
    s->vis_env[3] = s->pad_lvl;
}

static SDL_AudioSpec want_spec16 = {
    SAMPLE_RATE, AUDIO_F32LSB, 2, 0, 512, 0, 0, audio_cb16, &G
};

static void alloc_fbo_textures(GlApi16 *gl, uint32_t *tex, uint32_t *fbo, int w, int h) {
    for (int i = 0; i < 3; i++) {
        gl->BindTexture(GL_TEXTURE_2D, tex[i]);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, 0);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void demo16k_main(void) {
    Demo16State *s = &G;
    s->rng = 0x13579BDFu;

    /* Populate equal-tempered pitch table starting at D2 (73.4162 Hz) */
    for (int n = 0; n < 64; n++) {
        s->note_hz[n] = 73.4162f * exp2f((float)n * (1.0f / 12.0f));
    }

    load_dlsyms(dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_GLOBAL), sdl_names, (void **)&s->sdl);

    s->sdl.Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    void *win = s->sdl.CreateWindow("", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESK);
    s->sdl.GL_CreateContext(win);
    s->sdl.GL_SetSwapInterval(1);
    s->sdl.ShowCursor(0);
    load_glsyms(s->sdl.GL_GetProcAddress, gl_names, (void **)&s->gl);

    const char *src_scene = SHADER_SCENE_16K;
    const char *src_post  = SHADER_POST_16K;
    uint32_t prog_scene = s->gl.CreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &src_scene);
    uint32_t prog_post  = s->gl.CreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &src_post);

    int cur_w = 1920, cur_h = 1080;
    s->sdl.GL_GetDrawableSize(win, &cur_w, &cur_h);

    uint32_t tex[3], fbo[3];
    s->gl.GenTextures(3, tex);
    s->gl.GenFramebuffers(3, fbo);
    alloc_fbo_textures(&s->gl, tex, fbo, cur_w, cur_h);

    s->sdl.OpenAudio(&want_spec16, 0);
    s->sdl.PauseAudio(0);

    int t0 = s->sdl.GetTicks();
    int ping = 0;
    int frame_idx = 0;

    for (;;) {
        int ft0 = s->sdl.GetTicks();
        while (s->sdl.PollEvent(s->ev)) {
            if (s->ev[0] == SDL_QUIT_EV || (s->ev[0] == SDL_KEYDOWN_EV && s->ev[4] == SDL_SCANCODE_ESC)) {
                goto done;
            }
        }

        float t = (float)(ft0 - t0) * 0.001f;
        if (t >= 60.0f) break;

        int dw = 1920, dh = 1080;
        s->sdl.GL_GetDrawableSize(win, &dw, &dh);
        if (dw != cur_w || dh != cur_h) {
            cur_w = dw;
            cur_h = dh;
            alloc_fbo_textures(&s->gl, tex, fbo, cur_w, cur_h);
        }

        s->u[0] = (float)cur_w;
        s->u[1] = (float)cur_h;
        s->u[2] = t;
        s->u[3] = (float)(frame_idx++);
        for (int i = 0; i < 4; i++) s->u[4 + i] = s->vis_env[i];

        /* Pass 1: Render HDR raymarched abyssal scene into fbo[0] (tex[0]) */
        s->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
        s->gl.Viewport(0, 0, cur_w, cur_h);
        s->gl.UseProgram(prog_scene);
        s->gl.Uniform4fv(0, 3, s->u);
        s->gl.Rects(-1, -1, 1, 1);

        /* Pass 2: Temporal accumulation + pre-bloom into fbo[1 + ping] (reading tex[0] & tex[1 + (1 - ping)]) */
        s->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[1 + ping]);
        s->gl.UseProgram(prog_post);
        s->gl.ActiveTexture(GL_TEXTURE0);
        s->gl.BindTexture(GL_TEXTURE_2D, tex[0]);
        s->gl.ActiveTexture(GL_TEXTURE0 + 1u);
        s->gl.BindTexture(GL_TEXTURE_2D, tex[1 + (1 - ping)]);
        s->u[8] = 0.0f;
        s->gl.Uniform4fv(0, 3, s->u);
        s->gl.Rects(-1, -1, 1, 1);

        /* Pass 3: Wide optical bloom + chromatic dispersion + filmic composite to screen */
        s->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        s->gl.Viewport(0, 0, cur_w, cur_h);
        s->gl.ActiveTexture(GL_TEXTURE0 + 1u);
        s->gl.BindTexture(GL_TEXTURE_2D, tex[1 + ping]);
        s->u[8] = 1.0f;
        s->gl.Uniform4fv(0, 3, s->u);
        s->gl.Rects(-1, -1, 1, 1);

        ping ^= 1;
        s->sdl.GL_SwapWindow(win);

        if (s->sdl.GetTicks() - ft0 < 4) s->sdl.Delay(15u);
    }

done:
    s->sdl.Quit();
    sys_exit_zero();
}

DEMO_ENTRY(demo16k_main)
