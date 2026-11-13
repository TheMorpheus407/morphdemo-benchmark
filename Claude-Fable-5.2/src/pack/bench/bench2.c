// Round 2: fixed model set comparisons of mixing variants (bench only).
#define main cmbench_main
#include "cmbench.c"
#undef main
static int parse_masks(const char *s, int *ms) { int n = 0; while (*s) { ms[n++] = strtol(s, (char **)&s, 16); while (*s == ' ' || *s == ',') s++; } return n; }
// offline fit of static weights (multi-epoch SGD), returns static cost; w out
static double fit_static(const int *ms, int n, float delta, int kind, float *w, int epochs, float winit) {
    float L[256]; for (int i = 0; i < 256; i++) L[i] = logf(i + delta);
    float SQ[256]; for (int i = 0; i < 256; i++) SQ[i] = 1.0f / sqrtf(i + delta);
    for (int i = 0; i < n; i++) w[i] = winit;
    double bits = 0;
    for (int ep = 0; ep <= epochs; ep++) {
        float lr = ep == epochs ? 0 : 0.004f / (1 + ep);
        bits = 0; float st[64];
        for (int t = 0; t < nbits; t++) {
            float x = 0;
            for (int i = 0; i < n; i++) { const u8 *c = cnt[ms[i]] + 2 * t; float s = kind == 0 ? L[c[1]] - L[c[0]] : (float)(c[1] - c[0]) * SQ[c[0]] * SQ[c[1]]; st[i] = s; x += w[i] * s; }
            float p1 = kind == 0 ? 1.0f / (1.0f + expf(-x)) : 0.5f + 0.5f * x / sqrtf(1.0f + x * x);
            if (p1 < 1.0f / 65536) p1 = 1.0f / 65536; if (p1 > 1 - 1.0f / 65536) p1 = 1 - 1.0f / 65536;
            int bit = bitv[t]; bits -= log2(bit ? p1 : 1 - p1);
            float err = (bit - p1) * lr; for (int i = 0; i < n; i++) w[i] += err * st[i];
        }
    }
    return bits;
}
// adaptive with given per-model init weights
static double eval_lg_w(const int *ms, int n, float delta, int kind, const float *w0, float lr) {
    float L[256]; for (int i = 0; i < 256; i++) L[i] = logf(i + delta);
    float SQ[256]; for (int i = 0; i < 256; i++) SQ[i] = 1.0f / sqrtf(i + delta);
    float w[64]; memcpy(w, w0, sizeof(float) * n); double bits = 0; float st[64];
    for (int t = 0; t < nbits; t++) {
        float x = 0;
        for (int i = 0; i < n; i++) { const u8 *c = cnt[ms[i]] + 2 * t; float s = kind == 0 ? L[c[1]] - L[c[0]] : (float)(c[1] - c[0]) * SQ[c[0]] * SQ[c[1]]; st[i] = s; x += w[i] * s; }
        float p1 = kind == 0 ? 1.0f / (1.0f + expf(-x)) : 0.5f + 0.5f * x / sqrtf(1.0f + x * x);
        if (p1 < 1.0f / 65536) p1 = 1.0f / 65536; if (p1 > 1 - 1.0f / 65536) p1 = 1 - 1.0f / 65536;
        int bit = bitv[t]; bits -= log2(bit ? p1 : 1 - p1);
        float err = (bit - p1) * lr; for (int i = 0; i < n; i++) w[i] += err * st[i];
    }
    return bits;
}
// indirect: adaptive state maps from quantized (n0,n1) to p, then logistic mixing
static inline int q4(int n) { return n < 12 ? n : n < 20 ? 12 : n < 40 ? 13 : n < 100 ? 14 : 15; }
static double eval_ind(const int *ms, int n, float delta, float smrate, float lr, float winit, int sel, int smlimit) {
    float *sm = malloc(sizeof(float) * n * 256); u8 *smc = calloc(n, 256);
    for (int i = 0; i < n; i++) for (int a = 0; a < 16; a++) for (int b = 0; b < 16; b++) { static const float rep[16] = {0,1,2,3,4,5,6,7,8,9,10,11,15,28,60,180}; sm[i * 256 + a * 16 + b] = (rep[b] + delta) / (rep[a] + rep[b] + 2 * delta); }
    int nsel = sel ? 8 : 1; float w[8 * 64]; for (int i = 0; i < nsel * n; i++) w[i] = winit;
    double bits = 0; float st[64]; int idx[64];
    for (int t = 0; t < nbits; t++) {
        float *ww = w + (sel ? bpos[t] * n : 0); float x = 0;
        for (int i = 0; i < n; i++) { const u8 *c = cnt[ms[i]] + 2 * t; idx[i] = i * 256 + q4(c[0]) * 16 + q4(c[1]); float p = sm[idx[i]]; if (p < 0.0005f) p = 0.0005f; if (p > 0.9995f) p = 0.9995f; float s = (c[0] | c[1]) ? logf(p / (1 - p)) : 0; st[i] = s; x += ww[i] * s; }
        float p1 = 1.0f / (1.0f + expf(-x));
        if (p1 < 1.0f / 65536) p1 = 1.0f / 65536; if (p1 > 1 - 1.0f / 65536) p1 = 1 - 1.0f / 65536;
        int bit = bitv[t]; bits -= log2(bit ? p1 : 1 - p1);
        float err = (bit - p1) * lr; for (int i = 0; i < n; i++) ww[i] += err * st[i];
        for (int i = 0; i < n; i++) { float r = smrate; if (smlimit) { int k = smc[idx[i]]; r = 1.0f / (k + 1.5f); if (r < smrate) r = smrate; if (k < 255) smc[idx[i]] = k + 1; } sm[idx[i]] += (bit - sm[idx[i]]) * r; }
    }
    free(sm); free(smc); return bits;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    int ms[64]; int n = parse_masks(argv[2], ms);
    upd_rule = argc > 3 ? atoi(argv[3]) : 0; gen_all(0);
    printf("n=%d upd=%d\n", n, upd_rule);
    for (int kind = 0; kind <= 1; kind++) {
        float deltas[] = { 0.03f, 0.06f, 0.1f, 0.2f, 0.4f };
        for (int di = 0; di < 5; di++) {
            float d = deltas[di];
            printf("kind=%d delta=%.2f:", kind, d);
            float lrs[] = { 0.0005f, 0.001f, 0.002f, 0.004f, 0.008f };
            for (int wi = 0; wi < 3; wi++) { float winit = (kind ? 0.1f : 0.15f) * (1 << wi); double b = 1e30; float bl = 0; for (int li = 0; li < 5; li++) { LgP p = { kind, d, lrs[li], winit, 0, 0, 0 }; double v = eval_lg(ms, n, p); if (v < b) { b = v; bl = lrs[li]; } } printf("  adapt(w0=%.2f): %.1f (lr %.4f)", winit, b / 8, bl); }
            float w[64]; double s = fit_static(ms, n, d, kind, w, 6, kind ? 0.2f : 0.3f);
            printf("  | static-fit: %.1f", s / 8);
            double hb = 1e30; float hl = 0; for (int li = 0; li < 5; li++) { double v = eval_lg_w(ms, n, d, kind, w, lrs[li] * 0.5f); if (v < hb) { hb = v; hl = lrs[li] * 0.5f; } }
            printf("  hybrid: %.1f (lr %.4f)", hb / 8, hl);
            // quantized weights (1/32 steps)
            float wq[64]; for (int i = 0; i < n; i++) wq[i] = roundf(w[i] * 32) / 32; double hq = eval_lg_w(ms, n, d, kind, wq, hl);
            printf("  hybrid-q: %.1f\n", hq / 8);
            if (di == 2) { printf("     w:"); for (int i = 0; i < n; i++) printf(" %.2f", w[i]); printf("\n"); }
        }
    }
    float smr[] = { 0.02f, 0.05f, 0.1f };
    for (int ri = 0; ri < 3; ri++) for (int lim = 0; lim <= 1; lim++) for (int sel = 0; sel <= 1; sel++) {
        double b = 1e30; float bl = 0, bd = 0, bw = 0;
        for (int di = 0; di < 3; di++) for (int li = 0; li < 4; li++) for (int wi = 0; wi < 2; wi++) { float d = 0.1f * (1 << di), lr = 0.002f * (1 << li), w0 = 0.2f * (1 + wi); double v = eval_ind(ms, n, d, smr[ri], lr, w0, sel, lim); if (v < b) { b = v; bl = lr; bd = d; bw = w0; } }
        printf("indirect smrate=%.2f adaptive-rate=%d sel=%d: %.1f (delta %.2f lr %.4f w0 %.2f)\n", smr[ri], lim, sel, b / 8, bd, bl, bw);
    }
    return 0;
}
