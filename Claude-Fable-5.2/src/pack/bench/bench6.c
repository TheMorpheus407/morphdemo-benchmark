// CR with a fixed positional weight schedule w(i) (bench only).
#define main cmbench_main
#include "cmbench.c"
#undef main
static int sched = 0, gbase = 2, gboost = 0;
static int wpos(int i, int n) { switch (sched) { case 0: return i >> 1; case 1: return (i * 3) >> 2; case 2: return (i + 1) >> 1; case 3: return i < 12 ? i >> 1 : 6; case 4: return (i * 2 + 1) / 5; default: return i >> 1; } }
static double eval_s(const int *ms, int n) {
    double bits = 0; int w[64]; for (int i = 0; i < n; i++) w[i] = wpos(i, n);
    for (int t = 0; t < nbits; t++) {
        u64 n0 = gbase, n1 = gbase;
        for (int i = 0; i < n; i++) { const u8 *c = cnt[ms[i]] + 2 * t; int sh = w[i] + ((gboost && (c[0] == 0 || c[1] == 0)) ? 1 : 0); n0 += (u64)c[0] << sh; n1 += (u64)c[1] << sh; }
        bits += log2((double)(n0 + n1) / (bitv[t] ? n1 : n0));
    }
    return bits;
}
static double search(int *ms, int *pn, int maxn) {
    int n = 0; double best = 1e30;
    for (;;) {
        int improved = 0;
        if (n < maxn) {
            double cb[NM]; int cp[NM];
            #pragma omp parallel for schedule(dynamic)
            for (int m = 0; m < NM; m++) { cb[m] = 1e30; cp[m] = 0; int dup = 0; for (int i = 0; i < n; i++) if (ms[i] == m) dup = 1; if (dup) continue;
                for (int p = 0; p <= n; p++) { int tmp[64]; memcpy(tmp, ms, sizeof(int) * p); tmp[p] = m; memcpy(tmp + p + 1, ms + p, sizeof(int) * (n - p)); double b = eval_s(tmp, n + 1); if (b < cb[m]) { cb[m] = b; cp[m] = p; } } }
            int bm = -1; double bb = best - 8.0; for (int m = 0; m < NM; m++) if (cb[m] < bb) { bb = cb[m]; bm = m; }
            if (bm >= 0) { int p = cp[bm]; memmove(ms + p + 1, ms + p, sizeof(int) * (n - p)); ms[p] = bm; n++; best = bb; improved = 1; }
        }
        for (int i = 0; i < n && n > 1; i++) { int tmp[64], k = 0; for (int j = 0; j < n; j++) if (j != i) tmp[k++] = ms[j]; double b = eval_s(tmp, k); if (b < best + 8.0) { memcpy(ms, tmp, sizeof(int) * k); n = k; best = b; improved = 1; i--; } }
        for (int i = 0; i + 1 < n; i++) { int t = ms[i]; ms[i] = ms[i + 1]; ms[i + 1] = t; double b = eval_s(ms, n); if (b < best - 0.5) { best = b; improved = 1; } else { ms[i + 1] = ms[i]; ms[i] = t; } }
        if (!improved) break;
    }
    *pn = n; return best;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    int maxn = atoi(argv[2]); upd_rule = argc > 3 ? atoi(argv[3]) : 0; gen_all(0);
    for (gboost = 0; gboost <= 1; gboost++) for (sched = 0; sched <= 4; sched++) for (gbase = 2; gbase <= 4; gbase += 2) {
        int ms[64], n = 0; double b = search(ms, &n, maxn);
        printf("CRS upd=%d boost=%d sched=%d base=%d: %.1f bytes n=%d  masks:", upd_rule, gboost, sched, gbase, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x", ms[i]); printf("\n"); fflush(stdout);
    }
    return 0;
}
