#ifndef MORPH_SYNTH64_H
#define MORPH_SYNTH64_H

#include "../common/platform.h"

#define SR64        48000
#define STEP_SAMP   5625u       /* 128.0 BPM: 60 / 128 / 4 * 48000 = 5625 exact samples per 16th note! */
                                /* 32 bars * 16 steps * 5625 = 2,880,000 samples = exact 60.000s! */
#define DL64_LEN    65536u
#define DL64_MASK   (DL64_LEN - 1u)

/* Semitone offsets relative to C#2 / Db2 (MIDI 37, ~69.2957 Hz)
 * 0..3: Acts I-III C# minor dramatic progression (C#m9, Amaj7#11, F#m9, G#7sus4)
 * 4..7: Acts IV-V Db Major triumphant resolution (Dbmaj9, Bbm11, Gbmaj7#11, Abadd9)
 */
static const uint8_t chords64[8][6] = {
    { 0,  7, 12, 14, 15, 19},
    { 8, 12, 15, 19, 20, 26},
    { 5, 12, 16, 19, 20, 24},
    { 7, 12, 14, 17, 19, 24},
    { 0,  7, 11, 14, 16, 19},
    { 9, 12, 16, 19, 21, 24},
    { 5, 12, 16, 17, 21, 24},
    { 7, 14, 16, 19, 21, 26}
};

/* Soaring 32-step (2-bar) heroic lead melody in semitones above Db4 (+24), 255 = sustain/hold */
static const uint8_t lead_melody[32] = {
    16, 255, 255, 14, 16, 255, 19, 255,
    24, 255, 255, 23, 19, 255, 16, 255,
    17, 255, 255, 16, 14, 255, 11, 255,
    12, 255, 14, 255, 16, 255, 19, 255
};

static const uint8_t arp64_pat[16] = {
    0, 2, 3, 5, 4, 2, 1, 3, 0, 3, 4, 5, 3, 2, 4, 1
};

/* Prime-length FDN reverb delay lines (in samples at 48kHz: ~29ms to ~53ms) */
static const uint16_t fdn_lens[4] = { 1399, 1747, 2137, 2549 };

typedef struct {
    uint32_t sp;
    uint32_t rng;

    /* Drum & FX envelopes */
    float kick_env, kick_ph;
    float snr_env, snr_ph;
    float hat_env, hat_decay;
    float impact_env;

    /* Bass (3-osc unison Reese + sub + SVF) */
    float bass_ph[3];
    float bass_sub_ph;
    float bass_freq, bass_target;
    float bass_env;
    float bass_lp, bass_bp;

    /* 16th-note Arpeggiator (FM + dual saw pluck) */
    float arp_ph, arp_mod_ph;
    float arp_freq, arp_env;

    /* 6-voice Symphonic Strings / Brass Pad */
    float pad_ph[6][2];
    float pad_lvl;
    float pad_lp[2];

    /* Soaring Acts IV-V Lead Synth */
    float lead_ph[3];
    float lead_freq, lead_target, lead_env;
    float lead_lp;

    /* Noise filter states */
    float wn_lp, riser_bp, riser_lp;

    /* Stereo Ping-Pong Delay (dotted 8th = 16875 samples) */
    float dl_l[DL64_LEN];
    float dl_r[DL64_LEN];

    /* 4-channel Hadamard FDN Hall Reverb */
    float fdn_buf[4][4096];
    float fdn_lp[4];
    uint32_t fdn_idx[4];

    /* Pitch table & exported visual sync envelopes */
    float note_hz[64];
    float vis_kick;
    float vis_snr;
    float vis_arp;
    float vis_bass;
    float vis_lead;
    float vis_pad;
    float vis_flash;
} Synth64;

static inline float wrap01(float x) {
    return x - (float)(int)x;
}

static inline float fast_tanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static void synth64_init(Synth64 *s) {
    s->rng = 0xCAFEBABEu;
    for (int n = 0; n < 64; n++) {
        s->note_hz[n] = 69.29566f * exp2f((float)n * (1.0f / 12.0f));
    }
    s->bass_freq = s->bass_target = s->note_hz[0];
    s->lead_freq = s->lead_target = s->note_hz[40];
}

static void synth64_cb(void *ud, uint8_t *stream, int len) {
    Synth64 *s = (Synth64 *)ud;
    float *out = (float *)stream;
    int frames = len >> 3;

    for (int i = 0; i < frames; i++) {
        uint32_t sp   = s->sp++;
        uint32_t step = sp / STEP_SAMP;
        uint32_t spos = sp % STEP_SAMP;
        uint32_t bar  = step >> 4;
        uint32_t s16  = step & 15u;
        float t_sec   = (float)sp * (1.0f / 48000.0f);

        /* Bar 20 (t = 37.50s) is the Supernova Drop into Db Major! */
        int is_major  = (bar >= 20u);
        int is_riser  = (bar == 18u || bar == 19u);
        const uint8_t *ch = chords64[((bar >> 1) & 3u) + (is_major ? 4u : 0u)];

        uint32_t r = s->rng * 1664525u + 1013904223u;
        s->rng = r;
        float wn = ((float)(int32_t)r) * (1.0f / 2147483648.0f);

        if (spos == 0u) {
            /* Supernova drop impact at Bar 20 Beat 1 & Act transitions */
            if ((bar == 6u || bar == 13u || bar == 20u || bar == 25u) && s16 == 0u) {
                s->impact_env = (bar == 20u) ? 1.35f : 0.75f;
            }

            /* Kick drum choreography across 5 acts */
            uint16_t kpat = 0u;
            if (bar >= 3u && bar < 6u)        kpat = 0x0101u;           /* Act I late: half-time pulse */
            else if (bar >= 6u && bar < 13u)  kpat = 0x1089u;           /* Act II: syncopated breakbeat */
            else if (bar >= 13u && bar < 19u) kpat = 0x1111u;           /* Act III: 4-on-the-floor clockwork */
            else if (bar >= 20u && bar < 30u) kpat = 0x5115u;           /* Acts IV-V: driving anthem groove */
            if ((kpat >> s16) & 1u) {
                s->kick_env = 1.0f;
                s->kick_ph  = 0.0f;
            }

            /* Snare + Bar 19 crescendo roll */
            if (bar == 19u && (s16 >= 8u || (s16 & 1u) == 0u)) {
                s->snr_env = 0.45f + 0.04f * (float)s16;
                s->snr_ph  = 0.0f;
            } else if (bar >= 6u && bar < 30u && !is_riser && ((0x1010u >> s16) & 1u)) {
                s->snr_env = 0.95f;
                s->snr_ph  = 0.0f;
            }

            /* Precision clockwork hi-hats (open offbeat in Act III-V) */
            if (bar >= 8u && bar < 30u) {
                int open_hat = (bar >= 13u && ((s16 & 3u) == 2u));
                s->hat_env   = open_hat ? 0.72f : ((s16 & 1u) ? 0.28f : 0.44f);
                s->hat_decay = open_hat ? 0.99935f : 0.9972f;
            }

            /* Reese / FM Bassline */
            if (bar >= 4u && bar < 31u) {
                uint16_t bpat = (bar < 13u) ? 0x8181u : 0xFFFFu;
                if ((bpat >> s16) & 1u) {
                    uint8_t oct = (bar >= 13u && (s16 & 1u)) ? 12u : 0u;
                    uint8_t fifth = (s16 == 14u) ? 7u : 0u;
                    s->bass_target = s->note_hz[ch[0] + oct + fifth];
                    s->bass_env = ((s16 & 3u) == 0u) ? 1.0f : 0.72f;
                }
            }

            /* 16th-note Crystalline Arpeggiator */
            if (bar >= 6u && bar < 31u) {
                uint8_t vi = arp64_pat[s16];
                uint8_t hi = (bar >= 13u && ((s16 & 3u) == 3u)) ? 24u : 12u;
                s->arp_freq = s->note_hz[ch[vi] + hi];
                s->arp_env  = (bar < 13u) ? 0.58f : 0.85f;
            }

            /* Heroic Lead Melody in Acts IV-V (Bars 20..29) */
            if (bar >= 20u && bar < 30u) {
                uint8_t mn = lead_melody[step & 31u];
                if (mn != 255u) {
                    s->lead_target = s->note_hz[mn + 24u];
                    s->lead_env    = 0.95f;
                }
            }
        }

        /* Sidechain ducking pump from kick */
        float duck = 1.0f - 0.52f * s->kick_env * s->kick_env;

        /* 1. Emergence Singularity Static + Riser Sweep + Impact Boom */
        s->wn_lp += 0.24f * (wn - s->wn_lp);
        float hp_wn = wn - s->wn_lp;

        float chaos_amt = (t_sec < 12.0f) ? (1.0f - t_sec * (1.0f / 14.0f)) : 0.0f;
        float ris_prog = is_riser ? ((t_sec - 33.75f) * (1.0f / 3.75f)) : 0.0f;
        if (ris_prog < 0.0f) ris_prog = 0.0f;
        float rcut = 0.015f + 0.24f * ris_prog * ris_prog + 0.02f * chaos_amt * (0.5f + 0.5f * sinf(t_sec * 3.7f));
        s->riser_lp += rcut * s->riser_bp;
        s->riser_bp += rcut * (wn * 0.45f - s->riser_lp - 0.18f * s->riser_bp);

        float fx_sig = s->riser_bp * (0.42f * chaos_amt + 0.65f * ris_prog + 0.35f * s->impact_env);
        s->impact_env *= 0.99991f;

        float dry_l = fx_sig * 1.1f;
        float dry_r = -fx_sig * 0.9f;
        float rev_send_l = fx_sig * 0.45f;
        float rev_send_r = -fx_sig * 0.45f;

        /* 2. Layered Punchy Kick, Tonal Snare, & Clockwork Hi-Hats */
        float ke = s->kick_env;
        if (ke > 0.0005f) {
            float khz = 47.0f + 165.0f * ke * ke * ke * ke + 90.0f * (ke > 0.92f ? (ke - 0.92f) * 12.0f : 0.0f);
            s->kick_ph = wrap01(s->kick_ph + khz * (1.0f / 48000.0f));
            float ksig = fast_tanh(sinf(s->kick_ph * 6.2831853f) * 1.45f) * ke * 0.56f;
            dry_l += ksig;
            dry_r += ksig;
            s->kick_env = ke * 0.99932f;
        }

        float se = s->snr_env;
        if (se > 0.0005f) {
            float shz = 165.0f + 140.0f * se * se;
            s->snr_ph = wrap01(s->snr_ph + shz * (1.0f / 48000.0f));
            float body = sinf(s->snr_ph * 6.2831853f) * se * se * 0.28f;
            float snap = hp_wn * se * 0.34f;
            float ssig = body + snap;
            dry_l += ssig;
            dry_r += ssig;
            rev_send_l += snap * 0.55f;
            rev_send_r += snap * 0.55f;
            s->snr_env = se * 0.99915f;
        }

        float he = s->hat_env;
        if (he > 0.0005f) {
            float hsig = hp_wn * he * he * 0.19f;
            dry_l += hsig * 0.85f;
            dry_r += hsig * 1.15f;
            s->hat_env = he * s->hat_decay;
        }

        /* 3. Unison Detuned Reese + Clean Sub Bass */
        s->bass_freq += 0.0045f * (s->bass_target - s->bass_freq);
        if (bar >= 4u && s->bass_env > 0.001f) {
            float bf = s->bass_freq;
            s->bass_ph[0]  = wrap01(s->bass_ph[0] + bf * (0.994f / 48000.0f));
            s->bass_ph[1]  = wrap01(s->bass_ph[1] + bf * (1.000f / 48000.0f));
            s->bass_ph[2]  = wrap01(s->bass_ph[2] + bf * (1.006f / 48000.0f));
            s->bass_sub_ph = wrap01(s->bass_sub_ph + bf * (0.5f / 48000.0f));

            float reese = (s->bass_ph[0] + s->bass_ph[1] + s->bass_ph[2]) * (2.0f / 3.0f) - 1.0f;
            float bcut  = 0.032f + 0.145f * s->bass_env * (is_major ? 1.15f : 0.85f);
            s->bass_lp += bcut * s->bass_bp;
            s->bass_bp += bcut * (fast_tanh(reese * 1.6f) - s->bass_lp - 0.55f * s->bass_bp);

            float sub = sinf(s->bass_sub_ph * 6.2831853f) * 0.45f;
            float bsig = (s->bass_lp * 0.65f + sub) * s->bass_env * duck * 0.36f;
            dry_l += bsig;
            dry_r += bsig;
            s->bass_env *= 0.99968f;
        }

        /* 4. 16th-Note Crystalline FM + Saw Arpeggiator */
        float ae = s->arp_env;
        if (ae > 0.0008f) {
            float af = s->arp_freq;
            s->arp_ph     = wrap01(s->arp_ph + af * (1.0f / 48000.0f));
            s->arp_mod_ph = wrap01(s->arp_mod_ph + af * (is_major ? 2.0f : 3.0f) * (1.0f / 48000.0f));
            float fm = sinf((s->arp_ph + 0.28f * ae * ae * sinf(s->arp_mod_ph * 6.2831853f)) * 6.2831853f);
            float saw = s->arp_ph * 2.0f - 1.0f;
            float asig = (0.72f * fm + 0.28f * saw) * ae * duck * 0.16f;
            float pan = (step & 1u) ? 0.72f : 0.28f;
            dry_l += asig * pan;
            dry_r += asig * (1.0f - pan);
            rev_send_l += asig * (1.0f - pan) * 0.65f;
            rev_send_r += asig * pan * 0.65f;
            s->arp_env = ae * 0.99942f;
        }

        /* 5. 6-Voice Symphonic Strings / Brass Pad */
        float target_pad = (bar < 4u) ? 0.12f : (bar < 13u ? 0.42f : (is_major ? 0.95f : 0.68f));
        s->pad_lvl += 0.00012f * (target_pad - s->pad_lvl);
        {
            float pl = 0.0f, pr = 0.0f;
            for (int v = 0; v < 6; v++) {
                float pf = s->note_hz[ch[v] + 12u];
                float p0 = wrap01(s->pad_ph[v][0] + pf * (0.9975f / 48000.0f));
                float p1 = wrap01(s->pad_ph[v][1] + pf * (1.0025f / 48000.0f));
                s->pad_ph[v][0] = p0;
                s->pad_ph[v][1] = p1;
                float osc = (p0 * 2.0f - 1.0f) + (p1 * 2.0f - 1.0f);
                pl += osc * ((v & 1) ? 0.35f : 0.75f);
                pr += osc * ((v & 1) ? 0.75f : 0.35f);
            }
            float pcut = 0.045f + 0.065f * (is_major ? 1.0f : 0.45f) + 0.03f * sinf(t_sec * 0.35f);
            s->pad_lp[0] += pcut * (pl * 0.042f * s->pad_lvl * duck - s->pad_lp[0]);
            s->pad_lp[1] += pcut * (pr * 0.042f * s->pad_lvl * duck - s->pad_lp[1]);
            dry_l += s->pad_lp[0];
            dry_r += s->pad_lp[1];
            rev_send_l += s->pad_lp[0] * 0.55f;
            rev_send_r += s->pad_lp[1] * 0.55f;
        }

        /* 6. Soaring Heroic Lead Synth (Acts IV-V) */
        if (s->lead_env > 0.001f) {
            s->lead_freq += 0.0022f * (s->lead_target - s->lead_freq);
            float vibr = 1.0f + 0.0035f * sinf(t_sec * 36.0f);
            float lf = s->lead_freq * vibr;
            s->lead_ph[0] = wrap01(s->lead_ph[0] + lf * (0.996f / 48000.0f));
            s->lead_ph[1] = wrap01(s->lead_ph[1] + lf * (1.000f / 48000.0f));
            s->lead_ph[2] = wrap01(s->lead_ph[2] + lf * (1.004f / 48000.0f));
            float lsaw = ((s->lead_ph[0] + s->lead_ph[1] + s->lead_ph[2]) * (2.0f / 3.0f) - 1.0f);
            s->lead_lp += 0.22f * (fast_tanh(lsaw * 1.5f) - s->lead_lp);
            float lsig = s->lead_lp * s->lead_env * duck * 0.19f;
            dry_l += lsig;
            dry_r += lsig;
            rev_send_l += lsig * 0.75f;
            rev_send_r += lsig * 0.75f;
            s->lead_env *= 0.99993f;
        }

        /* 7. Tempo-Synced Dotted-8th Stereo Ping-Pong Delay (16,875 samples) */
        uint32_t widx = sp & DL64_MASK;
        uint32_t ridx = (sp - 16875u) & DL64_MASK;
        float dl = s->dl_l[ridx];
        float dr = s->dl_r[ridx];
        s->dl_l[widx] = fast_tanh(rev_send_l * 0.55f + dr * 0.46f);
        s->dl_r[widx] = fast_tanh(rev_send_r * 0.55f + dl * 0.46f);

        /* 8. 4-Delay Hadamard Feedback Delay Network (FDN) Cathedral Hall Reverb */
        float f_out[4];
        for (int c = 0; c < 4; c++) {
            uint32_t idx = s->fdn_idx[c];
            float rd = s->fdn_buf[c][idx];
            s->fdn_lp[c] += 0.38f * (rd - s->fdn_lp[c]);
            f_out[c] = s->fdn_lp[c];
        }
        /* Orthogonal Hadamard H4 feedback matrix * 0.5 * decay (0.84) */
        float s01 = f_out[0] + f_out[1], d01 = f_out[0] - f_out[1];
        float s23 = f_out[2] + f_out[3], d23 = f_out[2] - f_out[3];
        float fb0 = (s01 + s23) * 0.42f + rev_send_l * 0.35f;
        float fb1 = (d01 + d23) * 0.42f + rev_send_r * 0.35f;
        float fb2 = (s01 - s23) * 0.42f + dl * 0.25f;
        float fb3 = (d01 - d23) * 0.42f + dr * 0.25f;
        float fbs[4] = { fb0, fb1, fb2, fb3 };
        for (int c = 0; c < 4; c++) {
            uint32_t idx = s->fdn_idx[c];
            s->fdn_buf[c][idx] = fast_tanh(fbs[c]);
            idx++;
            if (idx >= fdn_lens[c]) idx = 0u;
            s->fdn_idx[c] = idx;
        }

        float wet_l = (f_out[0] + f_out[2]) * 0.38f + dl * 0.32f;
        float wet_r = (f_out[1] + f_out[3]) * 0.38f + dr * 0.32f;

        float mix_l = fast_tanh((dry_l + wet_l) * 0.88f);
        float mix_r = fast_tanh((dry_r + wet_r) * 0.88f);

        float fi = t_sec * 0.6f;
        float fo = (60.0f - t_sec) * 0.38f;
        float gain = fi < fo ? fi : fo;
        if (gain > 1.0f) gain = 1.0f;
        if (gain < 0.0f) gain = 0.0f;

        out[i * 2 + 0] = mix_l * gain;
        out[i * 2 + 1] = mix_r * gain;
    }

    s->vis_kick  = s->kick_env;
    s->vis_snr   = s->snr_env;
    s->vis_arp   = s->arp_env;
    s->vis_bass  = s->bass_env;
    s->vis_lead  = s->lead_env;
    s->vis_pad   = s->pad_lvl;
    s->vis_flash = s->impact_env;
}

#endif
