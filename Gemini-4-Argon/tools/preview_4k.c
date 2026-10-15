#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    void (*callback)(void *userdata, uint8_t *stream, int len);
    void *userdata;
} SDL_AudioSpec;

extern int SDL_Init(uint32_t flags);
extern void *SDL_CreateWindow(const char *title, int x, int y, int w, int h, uint32_t flags);
extern void *SDL_GL_CreateContext(void *window);
extern void SDL_DestroyWindow(void *window);
extern void SDL_Quit(void);

extern uint32_t glCreateShaderProgramv(uint32_t type, int count, const char **strings);
extern void glGetProgramiv(uint32_t program, uint32_t pname, int *params);
extern void glGetProgramInfoLog(uint32_t program, int maxLength, int *length, char *infoLog);
extern void glUseProgram(uint32_t program);
extern void glUniform4fv(int location, int count, const float *value);
extern void glRects(short x1, short y1, short x2, short y2);
extern void glReadPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *pixels);
extern void glViewport(int x, int y, int width, int height);
extern void glGenFramebuffers(int n, uint32_t *ids);
extern void glBindFramebuffer(uint32_t target, uint32_t fb);
extern void glGenTextures(int n, uint32_t *textures);
extern void glBindTexture(uint32_t target, uint32_t tex);
extern void glTexImage2D(uint32_t target, int level, int internalformat, int width, int height, int border, uint32_t format, uint32_t type, const void *pixels);
extern void glTexParameteri(uint32_t target, uint32_t pname, int param);
extern void glFramebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int level);

static float g_state[8];
static uint32_t g_rng = 0x13579BDFu;
static float g_delay_buf[24000];
static int g_delay_pos = 0;
static float g_lp1 = 0.0f, g_lp2 = 0.0f;

static const uint8_t g_notes[16] = {
    0, 3, 7, 12,
    8, 12, 15, 19,
    5, 8, 12, 17,
    7, 11, 14, 19
};

static float frand(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return (float)(int32_t)g_rng * (1.0f / 2147483648.0f);
}

static void synth_4k_step(float *out_l, float *out_r) {
    float t = g_state[0];
    g_state[0] = t + (1.0f / 48000.0f);

    float beat = t * 2.0f;          // 120 BPM
    int ibeat = (int)beat;
    float bf = beat - (float)ibeat;
    int step16 = (int)(beat * 4.0f);
    float sf = (beat * 4.0f) - (float)step16;
    int bar = (ibeat >> 2) & 3;

    float order = (t - 4.0f) * (1.0f / 34.0f);
    if (order < 0.0f) order = 0.0f;
    if (order > 1.0f) order = 1.0f;
    float climax = (t - 38.0f) * 0.2f;
    if (climax < 0.0f) climax = 0.0f;
    if (climax > 1.0f) climax = 1.0f;

    float n = frand();

    // 1. Emergence stochastic static & granular telemetry sweeps
    float chaos_gain = (1.0f - order * 0.88f);
    float grain_f = 180.0f + 620.0f * sinf(floorf(t * 12.0f) * 7.3f);
    float chaos_sig = (n * 0.35f + sinf(t * 6.28318f * grain_f) * expf(-sf * 18.0f) * 0.45f) * chaos_gain * (1.0f - climax);

    // 2. Kick drum (emerges at 8s, full at 16s..55s)
    float k_act = (t > 8.0f && t < 55.0f) ? ((t > 16.0f) ? 1.0f : 0.45f) : 0.0f;
    float k_env = expf(-bf * 9.5f) * k_act;
    float k_phase = 52.0f * bf - 3.2f * expf(-bf * 38.0f);
    float kick = sinf(6.283185f * k_phase) * k_env * 0.85f;

    // 3. Snare (beats 1 and 3 in Phase 2 & 3) & 16th hi-hats
    float s_act = (t > 18.0f && t < 54.0f && (ibeat & 1)) ? 1.0f : 0.0f;
    float s_env = expf(-bf * 16.0f) * s_act;
    float snare = (n * 0.7f + sinf(6.283185f * 185.0f * bf) * 0.4f) * s_env * 0.45f;
    float hh_act = (t > 16.0f && t < 56.0f) ? ((step16 & 2) ? 0.16f : 0.07f) : 0.0f;
    float hihat = n * expf(-sf * 42.0f) * hh_act;

    // 4. Bass (locks from unstable FM drift to solid harmonic root)
    int root_semi = g_notes[bar << 2];
    if (t > 48.0f) root_semi = 0; // Final D tonic pedal
    float drift = (1.0f - order) * 0.8f * sinf(t * 3.7f);
    float bass_freq = 73.416f * powf(2.0f, ((float)root_semi + drift) * (1.0f / 12.0f));
    float bass_env = (t > 16.0f) ? (0.4f + 0.6f * expf(-bf * 4.5f)) : (t * 0.05f);
    float bass_raw = sinf(6.283185f * bass_freq * t + 1.2f * sinf(6.283185f * bass_freq * 2.0f * t) * expf(-bf * 6.0f));
    float bass = bass_raw * bass_env * 0.42f;

    // 5. Arpeggiator & Climax Chords
    int arp_idx = (step16 + (step16 >> 2)) & 3;
    int semi = g_notes[(bar << 2) + arp_idx];
    if (t > 48.0f) {
        const int maj9[4] = {0, 4, 7, 14};
        semi = maj9[arp_idx];
    }
    float arp_freq = 146.832f * powf(2.0f, (float)semi * (1.0f / 12.0f));
    float arp_env = expf(-sf * (6.5f - 3.0f * climax)) * order;
    // Saw + pulse oscillator
    float saw = fmodf(t * arp_freq, 1.0f) * 2.0f - 1.0f;
    float saw2 = fmodf(t * arp_freq * 1.006f, 1.0f) * 2.0f - 1.0f;
    float cut = 0.08f + 0.35f * order + 0.25f * climax;
    g_lp1 += cut * ((saw + saw2) * 0.5f * arp_env - g_lp1);
    g_lp2 += cut * (g_lp1 - g_lp2);
    float lead = g_lp2 * 0.48f;

    // Shimmering pad chord in Resolution (38s..60s)
    float pad = 0.0f;
    if (climax > 0.01f) {
        const int c_semi[3] = {4, 7, 14};
        for (int c = 0; c < 3; c++) {
            int cs = (t > 48.0f) ? c_semi[c] : g_notes[(bar << 2) + c + 1];
            float pf = 146.832f * powf(2.0f, (float)cs * (1.0f / 12.0f));
            pad += sinf(6.283185f * pf * t + 0.4f * sinf(6.283185f * pf * 2.002f * t));
        }
        pad *= climax * 0.14f;
    }

    // Stereo delay for spatial width
    float del = g_delay_buf[g_delay_pos];
    g_delay_buf[g_delay_pos] = (lead + pad + chaos_sig * 0.4f) + del * 0.38f;
    g_delay_pos = (g_delay_pos + 1) % 18000; // 375ms dotted-8th stereo echo

    // Smooth master fade in (0..2s) and fade out (56.5..60s)
    float master = 1.0f;
    if (t < 2.0f) master = t * 0.5f;
    if (t > 56.5f) master = (60.0f - t) * (1.0f / 3.5f);
    if (master < 0.0f) master = 0.0f;

    float mix_l = (kick + snare + hihat * 0.8f + bass + lead + pad * 0.9f + chaos_sig + del * 0.35f) * master;
    float mix_r = (kick + snare + hihat * 1.2f + bass + lead * 0.85f + pad * 1.1f - chaos_sig * 0.6f + del * 0.45f) * master;

    // Soft saturation
    *out_l = mix_l / (1.0f + fabsf(mix_l));
    *out_r = mix_r / (1.0f + fabsf(mix_r));

    // Visual sync uniforms
    g_state[1] = k_env;
    g_state[2] = s_env;
    g_state[3] = bass_env;
    g_state[4] = arp_env;
    g_state[5] = order;
    g_state[6] = climax;
}

static void write_wav(const char *path, const float *samples, int num_frames) {
    FILE *f = fopen(path, "wb");
    int data_bytes = num_frames * 4;
    int file_size = 36 + data_bytes;
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    int fmt_len = 16;
    uint16_t audio_fmt = 1, chans = 2, block_align = 4, bits = 16;
    int rate = 48000, byte_rate = 48000 * 4;
    fwrite(&fmt_len, 4, 1, f);
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&chans, 2, 1, f);
    fwrite(&rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    for (int i = 0; i < num_frames * 2; i++) {
        float v = samples[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        int16_t s = (int16_t)(v * 32767.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *sf = fopen(argv[1], "rb");
    fseek(sf, 0, SEEK_END);
    long sz = ftell(sf);
    fseek(sf, 0, SEEK_SET);
    char *sh_src = calloc(1, sz + 1);
    fread(sh_src, 1, sz, sf);
    fclose(sf);

    SDL_Init(0x20);
    void *win = SDL_CreateWindow("preview", 0, 0, 1920, 1080, 0x00000002 | 0x00000008);
    SDL_GL_CreateContext(win);

    const char *src_ptr = sh_src;
    uint32_t prog = glCreateShaderProgramv(0x8B30, 1, &src_ptr);
    int linked = 0;
    glGetProgramiv(prog, 0x8B82, &linked);
    char log[4096] = {0};
    int log_len = 0;
    glGetProgramInfoLog(prog, sizeof(log) - 1, &log_len, log);
    if (!linked || log_len > 0) {
        printf("Shader log (linked=%d):\n%s\n", linked, log);
        if (!linked) return 1;
    }

    uint32_t fbo, tex;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(0x8D40, fbo);
    glGenTextures(1, &tex);
    glBindTexture(0x0DE1, tex);
    glTexImage2D(0x0DE1, 0, 0x8058, 1920, 1080, 0, 0x1908, 0x1401, 0);
    glTexParameteri(0x0DE1, 0x2801, 0x2601);
    glTexParameteri(0x0DE1, 0x2800, 0x2601);
    glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, tex, 0);
    glViewport(0, 0, 1920, 1080);
    glUseProgram(prog);

    int total_frames = 48000 * 60;
    float *audio_buf = malloc(sizeof(float) * total_frames * 2);
    float snap_times[8] = { 2.5f, 10.0f, 18.2f, 25.0f, 33.5f, 41.0f, 47.0f, 54.0f };
    int snap_idx = 0;
    uint8_t *pixels = malloc(1920 * 1080 * 3);

    for (int i = 0; i < total_frames; i++) {
        synth_4k_step(&audio_buf[i * 2], &audio_buf[i * 2 + 1]);
        if (snap_idx < 8 && g_state[0] >= snap_times[snap_idx]) {
            glUniform4fv(0, 2, g_state);
            glRects(-1, -1, 1, 1);
            glReadPixels(0, 0, 1920, 1080, 0x1907, 0x1401, pixels);
            char ppm_path[256];
            snprintf(ppm_path, sizeof(ppm_path), "build/snap_4k_%d.ppm", snap_idx);
            FILE *pf = fopen(ppm_path, "wb");
            fprintf(pf, "P6\n1920 1080\n255\n");
            for (int y = 1079; y >= 0; y--) {
                fwrite(pixels + y * 1920 * 3, 1, 1920 * 3, pf);
            }
            fclose(pf);
            printf("Rendered snapshot %d at t=%.1fs (kick=%.2f snare=%.2f bass=%.2f lead=%.2f)\n",
                   snap_idx, g_state[0], g_state[1], g_state[2], g_state[3], g_state[4]);
            snap_idx++;
        }
    }

    write_wav("build/preview_4k.wav", audio_buf, total_frames);
    printf("Saved 60s audio to build/preview_4k.wav\n");
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
