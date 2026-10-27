#include "audio.h"
#include <math.h>

#ifndef DEMO
#define DEMO 0
#endif

#define TAU 6.28318530717958647692f
#define DELAY_SIZE 24000

typedef struct {
    Uint32 rng;
    unsigned delay_pos;
    float noise_lp;
    float bass_lp;
    float pad_lp_l;
    float pad_lp_r;
    float delay_l[DELAY_SIZE];
    float delay_r[DELAY_SIZE];
} Synth;

static Synth synth;
static SDL_AudioDeviceID audio_device;

static float ramp(float x, float a, float b) {
    float v = (x - a) / (b - a);
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    return v * v * (3.f - 2.f * v);
}

static float noise(Synth *s) {
    Uint32 x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    return (float)(x >> 8) * (2.f / 16777216.f) - 1.f;
}

static float note(float n) {
    return 440.f * powf(2.f, (n - 69.f) * (1.f / 12.f));
}

static float soft(float x) {
    return x / (1.f + fabsf(x));
}

#if DEMO == 0
static const signed char roots[8] = {45, 45, 41, 41, 48, 48, 43, 43};
static const signed char bass_steps[8] = {0, 0, 12, 0, 7, 12, 3, 7};
static const signed char arp_steps[16] = {0, 7, 12, 15, 19, 15, 12, 7, 3, 10, 14, 17, 22, 17, 14, 10};
#endif
#if DEMO == 1
static const signed char roots[8] = {48, 43, 45, 41, 48, 53, 43, 41};
static const signed char bells[16] = {12, 19, 24, 28, 19, 24, 16, 19, 14, 21, 26, 31, 26, 21, 19, 16};
#endif
#if DEMO == 2
static const signed char roots[4] = {38, 38, 34, 36};
static const signed char arp_steps[8] = {0, 12, 7, 15, 3, 19, 10, 14};
#endif

static void render_audio(Synth *s, float *out, int frames) {
    for (int i = 0; i < frames; ++i) {
        int position = SDL_AtomicGet(&demo_clock);
        float t = (float)position * (1.f / AUDIO_RATE);
        float n = noise(s);
        s->noise_lp += .12f * (n - s->noise_lp);
        float high = n - s->noise_lp;
        float left = 0.f, right = 0.f;
        float send = 0.f;
        float master = ramp(t, 0.f, .35f) * (1.f - ramp(t, 57.8f, 60.f));

#if DEMO == 0
        float beat = t * 2.f;
        int bi = (int)beat;
        float bp = beat - bi;
        float bs = bp * .5f;
        int step = (int)(beat * 4.f);
        float sp = beat * 4.f - step;
        float ns = sp * .125f;
        int bar = bi >> 2;
        int root = roots[bar & 7];
        float order = ramp(t, 10.f, 24.f);
        float rhythm = ramp(t, 8.f, 18.f) * (1.f - ramp(t, 55.f, 59.f));

        float air = high * (.12f - .085f * order) * (.55f + .45f * sinf(t * .73f));
        left += air;
        right += s->noise_lp * (.09f - .06f * order) + high * .025f;

        if (t > 7.f || (bi & 3) == 0) {
            float phase = 47.f * bs + 6.5f * (1.f - expf(-25.f * bs));
            float kick = sinf(TAU * phase) * expf(-9.f * bs);
            float kg = ((bi & 3) == 0 || t > 8.f) ? rhythm : .5f;
            left += kick * .48f * kg;
            right += kick * .48f * kg;
        }
        if (t > 15.f && ((bi & 3) == 1 || (bi & 3) == 3)) {
            float sn = (high * .75f + sinf(TAU * 176.f * bs) * .25f) * expf(-17.f * bs);
            left += sn * .16f * rhythm;
            right += sn * .19f * rhythm;
            send += sn * .09f;
        }
        if (t > 19.f && (step & 1)) {
            float hat = high * expf(-48.f * ns) * (.4f + .6f * (float)((step * 17) & 3) / 3.f);
            left += hat * .065f * rhythm;
            right += hat * .105f * rhythm;
        }

        float ep = beat * 2.f;
        int ei = (int)ep;
        float ef = ep - ei;
        float ens = ef * .25f;
        float bf = note(root - 12 + bass_steps[ei & 7]);
        float bph = bf * ens;
        float bass_raw = sinf(TAU * bph) * .8f + (2.f * (bph - floorf(bph)) - 1.f) * .2f;
        s->bass_lp += .22f * (bass_raw - s->bass_lp);
        float bass = s->bass_lp * expf(-2.3f * ef) * (1.f - expf(-90.f * ens));
        float bg = ramp(t, 11.f, 20.f) * (1.f - .55f * ramp(t, 53.f, 59.f));
        left += bass * .28f * bg;
        right += bass * .28f * bg;

        float ap = beat * 4.f;
        int ai = (int)ap;
        float af = ap - ai;
        float ans = af * .125f;
        int arp_note = root + 12 + arp_steps[ai & 15];
        float freq = note(arp_note);
        float ph = freq * ans;
        float pluck = (sinf(TAU * ph) + .32f * sinf(TAU * ph * 2.001f)) * expf(-4.8f * af);
        pluck *= 1.f - expf(-120.f * ans);
        float ag = ramp(t, 17.f, 28.f) * (1.f - ramp(t, 56.f, 60.f));
        float pan = .25f + .5f * (float)(ai & 3) / 3.f;
        left += pluck * .13f * ag * (1.f - pan);
        right += pluck * .13f * ag * pan;
        send += pluck * .16f * ag;

        if (t < 18.f && ((ai * 13 + 7) & 15) < 3) {
            float sparse = sinf(TAU * note(69 + arp_steps[ai & 15]) * ans) * expf(-7.f * af);
            left += sparse * .075f * (1.f - order);
            right += sparse * .11f * (1.f - order);
            send += sparse * .12f;
        }

        float pad_l = 0.f, pad_r = 0.f;
        for (int k = 0; k < 3; ++k) {
            float pf = note(root + (k == 0 ? 0 : (k == 1 ? 7 : 15)));
            pad_l += sinf(TAU * pf * t + .18f * sinf(t * .31f + k));
            pad_r += sinf(TAU * pf * 1.0013f * t + .18f * sinf(t * .27f + k + 1));
        }
        s->pad_lp_l += .018f * (pad_l * .12f - s->pad_lp_l);
        s->pad_lp_r += .018f * (pad_r * .12f - s->pad_lp_r);
        float pg = ramp(t, 4.f, 16.f) * (.65f + .35f * ramp(t, 40.f, 49.f));
        left += s->pad_lp_l * pg;
        right += s->pad_lp_r * pg;
        send += (s->pad_lp_l + s->pad_lp_r) * .06f;
#endif

#if DEMO == 1
        float beat = t * 1.6f;
        int bi = (int)beat;
        float bp = beat - bi;
        float bs = bp * .625f;
        int bar = bi >> 2;
        int root = roots[bar & 7];
        float order = ramp(t, 9.f, 25.f);
        float rhythm = ramp(t, 14.f, 28.f) * (1.f - ramp(t, 55.f, 59.5f));
        float wash = high * (.095f - .07f * order) * (.6f + .4f * sinf(t * .41f));
        left += wash * .8f;
        right += wash;

        if (t > 12.f || (bi & 7) == 0) {
            float phase = 43.f * bs + 4.f * (1.f - expf(-18.f * bs));
            float kick = sinf(TAU * phase) * expf(-7.f * bs);
            float kg = (bi & 3) == 0 ? .8f : .3f;
            left += kick * .38f * rhythm * kg;
            right += kick * .38f * rhythm * kg;
        }
        if (t > 22.f && (bi & 1)) {
            float rim = (high * .3f + sinf(TAU * 320.f * bs) * .7f) * expf(-28.f * bs);
            left += rim * .12f * rhythm;
            right += rim * .09f * rhythm;
            send += rim * .12f;
        }

        float pp = beat * 2.f;
        int pi = (int)pp;
        float pf = pp - pi;
        float pns = pf * .3125f;
        int ni = pi & 15;
        float freq = note(root + bells[ni]);
        float phase = freq * pns;
        float bell = (sinf(TAU * phase) + .45f * sinf(TAU * phase * 2.004f) + .18f * sinf(TAU * phase * 3.99f));
        bell *= expf(-3.4f * pf) * (1.f - expf(-80.f * pns));
        float bg = ramp(t, 8.f, 23.f) * (1.f - .5f * ramp(t, 54.f, 60.f));
        float pan = .5f + .42f * sinf((float)ni * 2.3f);
        left += bell * .105f * bg * (1.f - pan);
        right += bell * .105f * bg * pan;
        send += bell * .24f * bg;

        float bp2 = beat;
        float bass_phase = note(root - 12) * bs;
        float bass = sinf(TAU * bass_phase) * expf(-2.8f * bp2);
        s->bass_lp += .16f * (bass - s->bass_lp);
        float basg = ramp(t, 15.f, 27.f);
        left += s->bass_lp * .24f * basg;
        right += s->bass_lp * .24f * basg;

        if (t > 25.f && (pi & 1)) {
            float hat = high * expf(-55.f * pns);
            left += hat * .035f * rhythm;
            right += hat * .075f * rhythm;
        }

        float pad_l = 0.f, pad_r = 0.f;
        for (int k = 0; k < 3; ++k) {
            float f = note(root + (k == 0 ? 0 : (k == 1 ? 7 : 16)));
            pad_l += sinf(TAU * f * t + .25f * sinf(t * .23f + k));
            pad_r += sinf(TAU * f * .9987f * t + .25f * sinf(t * .29f + k + 2));
        }
        s->pad_lp_l += .012f * (pad_l * .11f - s->pad_lp_l);
        s->pad_lp_r += .012f * (pad_r * .11f - s->pad_lp_r);
        float pg = ramp(t, 2.f, 15.f) * (.7f + .3f * sinf(t * .19f));
        left += s->pad_lp_l * pg;
        right += s->pad_lp_r * pg;
        send += (s->pad_lp_l + s->pad_lp_r) * .1f;
#endif

#if DEMO == 2
        float beat = t * (32.f / 15.f);
        int bi = (int)beat;
        float bp = beat - bi;
        float bs = bp * .46875f;
        int root = roots[(bi >> 2) & 3];
        float order = ramp(t, 8.f, 24.f);
        float drums = ramp(t, 10.f, 22.f) * (1.f - ramp(t, 55.f, 59.f));
        left += high * (.11f - .085f * order);
        right += high * (.09f - .068f * order);

        if (t > 10.f || !(bi & 3)) {
            float k = sinf(TAU * (45.f * bs + 5.f * (1.f - expf(-24.f * bs)))) * expf(-10.f * bs);
            left += k * .42f * drums;
            right += k * .42f * drums;
        }
        float sp = beat * 4.f;
        int si = (int)sp;
        float sf = sp - si;
        float ss = sf * .1171875f;
        if (t > 19.f && (si & 1)) {
            float h = high * expf(-60.f * ss) * .08f * drums;
            left += h;
            right += h * .7f;
        }
        float ep = beat * 2.f;
        int ei = (int)ep;
        float ef = ep - ei;
        float es = ef * .234375f;
        float bf = note(root + ((ei & 2) ? 7 : 0));
        float bph = bf * es;
        float b = (sinf(TAU * bph) * .75f + (2.f * (bph - floorf(bph)) - 1.f) * .25f) * expf(-3.f * ef);
        s->bass_lp += .2f * (b - s->bass_lp);
        float bg = ramp(t, 12.f, 23.f);
        left += s->bass_lp * .3f * bg;
        right += s->bass_lp * .3f * bg;
        float f = note(root + 24 + arp_steps[si & 7]);
        float p = sinf(TAU * f * ss) * expf(-6.f * sf);
        float ag = ramp(t, 17.f, 29.f) * (1.f - ramp(t, 56.f, 60.f));
        left += p * .13f * ag * (si & 1 ? .3f : .8f);
        right += p * .13f * ag * (si & 1 ? .8f : .3f);
        send += p * .16f * ag;
        float pad = sinf(TAU * note(root + 12) * t) + sinf(TAU * note(root + 19) * t);
        s->pad_lp_l += .015f * (pad * .1f - s->pad_lp_l);
        left += s->pad_lp_l * ramp(t, 5.f, 20.f);
        right += s->pad_lp_l * ramp(t, 7.f, 22.f);
#endif

        unsigned dp = s->delay_pos;
        float dl = s->delay_l[dp];
        float dr = s->delay_r[dp];
        left += dr * .24f;
        right += dl * .24f;
        s->delay_l[dp] = soft(left * .18f + send + dr * .31f);
        s->delay_r[dp] = soft(right * .18f + send + dl * .31f);
        if (++dp == DELAY_SIZE) dp = 0;
        s->delay_pos = dp;

        out[i * 2] = soft(left * .82f) * master;
        out[i * 2 + 1] = soft(right * .82f) * master;
        SDL_AtomicSet(&demo_clock, position + 1);
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int length) {
    render_audio((Synth *)userdata, (float *)stream, length / 8);
}

SDL_AudioDeviceID audio_open(void) {
    SDL_AudioSpec want = {0}, got;
    synth.rng = 0x92d68ca2u;
    want.freq = AUDIO_RATE;
    want.format = AUDIO_F32LSB;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = &synth;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    return audio_device;
}

void audio_resume(SDL_AudioDeviceID device) {
    SDL_PauseAudioDevice(device, 0);
}
