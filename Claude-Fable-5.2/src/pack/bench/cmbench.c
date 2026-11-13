// Bench for choosing the context-mixing model family. Not part of the shipped tool.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <omp.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
static u8 *data; static int len, nbits;
#define NM 256
static u8 *cnt[NM];            // per mask: (n0,n1) before each bit
static float *ema[NM];         // per mask: logit-ema state before each bit
static u8 *bitv;               // the bits
static u8 *bpos;               // bit position 0..7
static u8 *part;               // partial byte (1..255)
static int upd_rule = 0;       // counter update rule variant
static float ema_rate = 0.25f;

static inline u64 mix64(u64 x){ x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return x; }
typedef struct { u64 key; u32 idx; } HE;
static void gen_mask(int m, int want_ema) {
    int cap = 1; while (cap < nbits * 2) cap <<= 1;
    HE *ht = calloc(cap, sizeof(HE)); u8 *st = calloc(nbits + 1, 2); float *es = want_ema ? calloc(nbits + 1, sizeof(float)) : 0; u32 nst = 1;
    u8 *out = cnt[m] = realloc(cnt[m], 2 * (size_t)nbits);
    float *eo = 0; if (want_ema) eo = ema[m] = realloc(ema[m], sizeof(float) * (size_t)nbits);
    int t = 0;
    for (int pos = 0; pos < len; pos++) {
        u64 ch = 0x9E3779B97F4A7C15ULL * (m + 1);
        for (int j = 0; j < 8; j++) if (m >> j & 1) { u8 c = pos - 1 - j >= 0 ? data[pos - 1 - j] : 0; ch = mix64(ch ^ (c + 1) ^ ((u64)j << 32)); }
        int partial = 1;
        for (int b = 7; b >= 0; b--, t++) {
            u64 key = mix64(ch ^ ((u64)partial << 40)) | 1;
            u32 h = (u32)(key >> 20) & (cap - 1);
            while (ht[h].key && ht[h].key != key) h = (h + 1) & (cap - 1);
            if (!ht[h].key) { ht[h].key = key; ht[h].idx = nst++; }
            u8 *s = st + 2 * ht[h].idx;
            out[2 * t] = s[0]; out[2 * t + 1] = s[1];
            int bit = data[pos] >> b & 1;
            if (want_ema) { float *e = es + ht[h].idx; eo[t] = *e; *e += ((bit ? 1.0f : -1.0f) - *e) * ema_rate; }
            // counter update
            if (s[bit] < 255) s[bit]++;
            switch (upd_rule) {
            case 0: if (s[!bit] > 1) s[!bit] >>= 1; break;                 // crinkler
            case 1: if (s[!bit] > 2) s[!bit] = s[!bit] / 2 + 1; break;     // paq1
            case 2: if (s[!bit] > 1) s[!bit] = (s[!bit] + 1) >> 1; break;
            case 3: s[!bit] = (s[!bit] + 1) >> 1; break;                   // aggressive: 1->1, 2->1, 3->2
            case 4: s[!bit] >>= 1; break;                                  // very aggressive: 1->0
            case 5: break;                                                 // stationary
            case 6: if (s[!bit] == 0 && s[bit] < 255) s[bit]++; s[!bit] = (s[!bit] + 1) >> 1; break;   // det: +2
            case 7: if (s[!bit] == 0 && s[bit] < 255) s[bit]++; if (s[!bit] > 1) s[!bit] >>= 1; break;
            case 8: if (s[!bit] == 0 && s[bit] < 254) s[bit] += 2; s[!bit] = (s[!bit] + 1) >> 1; break; // det: +3
            }
            partial = partial * 2 + bit;
        }
    }
    free(ht); free(st); free(es);
}
static void gen_all(int want_ema) {
    #pragma omp parallel for schedule(dynamic)
    for (int m = 0; m < NM; m++) gen_mask(m, want_ema);
}
// ---------- Crinkler style ----------
typedef struct { int base, boost; } CrP;
static double eval_cr(const int *ms, const int *ws, int n, CrP p) {
    double bits = 0;
    for (int t = 0; t < nbits; t++) {
        u32 n0 = p.base, n1 = p.base;
        for (int i = 0; i < n; i++) {
            const u8 *c = cnt[ms[i]] + 2 * t;
            int sh = ws[i] + ((c[0] == 0 || c[1] == 0) ? p.boost : 0);
            n0 += (u32)c[0] << sh; n1 += (u32)c[1] << sh;
        }
        bits += log2((double)(n0 + n1) / (bitv[t] ? n1 : n0));
    }
    return bits;
}
// ---------- logistic ----------
typedef struct { int kind;   // 0: log stretch of counts, 1: algebraic of counts, 2: logit-ema, 3: ema w/ algebraic squash
                 float delta, lr, winit; int sel; /* 0 none, 1 bitpos, 2 partial */ int apm; float lr_decay; } LgP;
static float Ltab[256];
static double eval_lg(const int *ms, int n, LgP p) {
    int nsel = p.sel == 0 ? 1 : p.sel == 1 ? 8 : 256;
    float *w = alloca(sizeof(float) * nsel * n);
    for (int i = 0; i < nsel * n; i++) w[i] = p.winit;
    float L[256]; for (int i = 0; i < 256; i++) L[i] = logf(i + p.delta);
    float SQ[256]; for (int i = 0; i < 256; i++) SQ[i] = 1.0f / sqrtf(i + p.delta);
    double bits = 0; float st[64];
    float lr = p.lr;
    for (int t = 0; t < nbits; t++) {
        float *ww = w + n * (p.sel == 0 ? 0 : p.sel == 1 ? bpos[t] : part[t]);
        float x = 0;
        for (int i = 0; i < n; i++) {
            float s;
            if (p.kind == 0) { const u8 *c = cnt[ms[i]] + 2 * t; s = L[c[1]] - L[c[0]]; }
            else if (p.kind == 1) { const u8 *c = cnt[ms[i]] + 2 * t; s = (float)(c[1] - c[0]) * SQ[c[0]] * SQ[c[1]]; }
            else s = ema[ms[i]][t];
            st[i] = s; x += ww[i] * s;
        }
        float p1;
        if (p.kind == 1 || p.kind == 3) p1 = 0.5f + 0.5f * x / sqrtf(1.0f + x * x);
        else p1 = 1.0f / (1.0f + expf(-x));
        if (p1 < 1.0f / 65536) p1 = 1.0f / 65536; if (p1 > 1 - 1.0f / 65536) p1 = 1 - 1.0f / 65536;
        int bit = bitv[t];
        bits -= log2(bit ? p1 : 1 - p1);
        float err = (bit - p1) * lr;
        for (int i = 0; i < n; i++) ww[i] += err * st[i];
        if (p.lr_decay > 0) lr = p.lr * (1.0f + p.lr_decay * 1000.0f / (1000.0f + t)) / (1.0f + p.lr_decay) ;
    }
    return bits;
}
// ---------- generic greedy search ----------
typedef double (*EvalFn)(const int *ms, int n, void *ctx);
static double ev_lg(const int *ms, int n, void *ctx) { return eval_lg(ms, n, *(LgP *)ctx); }
static int crw[64]; // weights for crinkler search
static double ev_cr(const int *ms, int n, void *ctx) { return eval_cr(ms, crw, n, *(CrP *)ctx); }
static double greedy(EvalFn f, void *ctx, int *ms, int *pn, int maxn, int is_cr, int verbose) {
    int n = *pn; double best = n ? f(ms, n, ctx) : 1e30;
    for (;;) {
        int improved = 0;
        // try add
        if (n < maxn) {
            double cb[NM]; int cw[NM];
            #pragma omp parallel for schedule(dynamic)
            for (int m = 0; m < NM; m++) {
                cb[m] = 1e30; cw[m] = 0;
                int dup = 0; for (int i = 0; i < n; i++) if (ms[i] == m) dup = 1;
                if (dup) continue;
                int tmp[64]; memcpy(tmp, ms, sizeof(int) * n); tmp[n] = m;
                if (is_cr) { int tw[64]; memcpy(tw, crw, sizeof(int) * n);
                    for (int w = 0; w <= 5; w++) { tw[n] = w; double b = eval_cr(tmp, tw, n + 1, *(CrP *)ctx); if (b < cb[m]) { cb[m] = b; cw[m] = w; } } }
                else cb[m] = f(tmp, n + 1, ctx);
            }
            int bm = -1; double bb = best - 4.0 * (is_cr ? 1 : 1) * 8 * 0 - 8.0; // must gain at least one byte (the mask byte)
            for (int m = 0; m < NM; m++) if (cb[m] < bb) { bb = cb[m]; bm = m; }
            if (bm >= 0) { ms[n] = bm; if (is_cr) crw[n] = cw[bm]; n++; best = bb; improved = 1; if (verbose) fprintf(stderr, "  +%02x -> %.1f bytes (n=%d)\n", bm, best / 8, n); }
        }
        // try remove
        for (int i = 0; i < n && n > 1; i++) {
            int tmp[64], tw[64], k = 0; for (int j = 0; j < n; j++) if (j != i) { tmp[k] = ms[j]; tw[k] = crw[j]; k++; }
            double b; if (is_cr) b = eval_cr(tmp, tw, k, *(CrP *)ctx); else b = f(tmp, k, ctx);
            if (b < best + 8.0) { if (verbose) fprintf(stderr, "  -%02x -> %.1f bytes\n", ms[i], b / 8); memcpy(ms, tmp, sizeof(int) * k); memcpy(crw, tw, sizeof(int) * k); n = k; best = b; improved = 1; i--; }
        }
        // crinkler: tune weights
        if (is_cr) for (int i = 0; i < n; i++) for (int d = -1; d <= 1; d += 2) {
            int old = crw[i]; if (old + d < 0 || old + d > 6) continue; crw[i] = old + d;
            double b = eval_cr(ms, crw, n, *(CrP *)ctx); if (b < best - 0.5) { best = b; improved = 1; } else crw[i] = old;
        }
        if (!improved) break;
    }
    *pn = n; return best;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    const char *mode = argc > 2 ? argv[2] : "all";
    int maxn = argc > 3 ? atoi(argv[3]) : 16;
    fprintf(stderr, "%s: %d bytes\n", argv[1], len);
    if (!strcmp(mode, "cr") || !strcmp(mode, "all")) {
        for (upd_rule = 0; upd_rule <= 2; upd_rule++) { gen_all(0);
            for (int boost = 1; boost <= 3; boost++) for (int base = 1; base <= 4; base *= 2) {
                CrP p = { base, boost }; int ms[64], n = 0; double b = greedy(ev_cr, &p, ms, &n, maxn, 1, 0);
                printf("CR upd=%d boost=%d base=%d: %.1f bytes n=%d  masks:", upd_rule, boost, base, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x/%d", ms[i], crw[i]); printf("\n"); fflush(stdout);
            } }
    }
    if (!strcmp(mode, "lg") || !strcmp(mode, "all")) {
        for (int ui = 0; ui < 3; ui++) { upd_rule = ui == 0 ? 0 : ui == 1 ? 3 : 4; gen_all(0);
            for (int kind = 0; kind <= 1; kind++) for (int sel = 0; sel <= 1; sel++) {
                float deltas[] = { 0.2f, 0.4f, 0.8f }; float lrs[] = { 0.002f, 0.005f, 0.012f, 0.03f };
                double bestb = 1e30; LgP bp = {0}; int bms[64], bn = 0;
                for (int di = 0; di < 3; di++) for (int li = 0; li < 4; li++) {
                    LgP p = { kind, deltas[di], lrs[li], kind == 0 ? 0.3f : 0.2f, sel, 0, 0 }; int ms[64], n = 0;
                    double b = greedy(ev_lg, &p, ms, &n, maxn, 0, 0);
                    if (b < bestb) { bestb = b; bp = p; bn = n; memcpy(bms, ms, sizeof ms); }
                }
                printf("LG upd=%d kind=%d sel=%d: %.1f bytes n=%d delta=%.2f lr=%.4f masks:", upd_rule, kind, sel, bestb / 8, bn, bp.delta, bp.lr); for (int i = 0; i < bn; i++) printf(" %02x", bms[i]); printf("\n"); fflush(stdout);
            } }
    }
    if (!strcmp(mode, "ema") || !strcmp(mode, "all")) {
        float rates[] = { 0.15f, 0.25f, 0.35f, 0.5f };
        for (int ri = 0; ri < 4; ri++) { ema_rate = rates[ri]; gen_all(1);
            for (int kind = 2; kind <= 3; kind++) for (int sel = 0; sel <= 1; sel++) {
                float lrs[] = { 0.02f, 0.05f, 0.12f, 0.3f }; float wis[] = { 1.0f, 2.0f, 4.0f };
                double bestb = 1e30; LgP bp = {0}; int bms[64], bn = 0;
                for (int wi = 0; wi < 3; wi++) for (int li = 0; li < 4; li++) {
                    LgP p = { kind, 0, lrs[li], wis[wi], sel, 0, 0 }; int ms[64], n = 0;
                    double b = greedy(ev_lg, &p, ms, &n, maxn, 0, 0);
                    if (b < bestb) { bestb = b; bp = p; bn = n; memcpy(bms, ms, sizeof ms); }
                }
                printf("EMA rate=%.2f kind=%d sel=%d: %.1f bytes n=%d winit=%.1f lr=%.3f masks:", ema_rate, kind, sel, bestb / 8, bn, bp.winit, bp.lr); for (int i = 0; i < bn; i++) printf(" %02x", bms[i]); printf("\n"); fflush(stdout);
            } }
    }
    return 0;
}
