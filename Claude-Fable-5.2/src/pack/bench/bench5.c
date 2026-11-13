// CR with implicit weights derived from popcount(mask) (bench only).
#define main cmbench_main
#include "cmbench.c"
#undef main
static int wrule = 0, gbase = 2;
static int wof(int m) { int pc = __builtin_popcount(m); switch (wrule) { case 0: return pc; case 1: return pc > 0 ? pc - 1 : 0; case 2: return pc > 6 ? 6 : pc; case 3: return (pc * 3 + 1) / 4; case 4: return pc + 1; default: return pc; } }
static double eval_crp(const int *ms, int n, void *ctx) {
    double bits = 0; int w[64]; for (int i = 0; i < n; i++) w[i] = wof(ms[i]);
    for (int t = 0; t < nbits; t++) {
        u64 n0 = gbase, n1 = gbase;
        for (int i = 0; i < n; i++) { const u8 *c = cnt[ms[i]] + 2 * t; n0 += (u64)c[0] << w[i]; n1 += (u64)c[1] << w[i]; }
        bits += log2((double)(n0 + n1) / (bitv[t] ? n1 : n0));
    }
    return bits;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    int maxn = atoi(argv[2]);
    int rules[] = { 0, 2 };
    for (int ri = 0; ri < 2; ri++) { upd_rule = rules[ri]; gen_all(0);
        for (wrule = 0; wrule <= 4; wrule++) for (gbase = 1; gbase <= 4; gbase *= 2) {
            int ms[64], n = 0; double b = greedy(eval_crp, 0, ms, &n, maxn, 0, 0);
            printf("CRP upd=%d wrule=%d base=%d: %.1f bytes n=%d  masks:", upd_rule, wrule, gbase, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x", ms[i]); printf("\n"); fflush(stdout);
        } }
    return 0;
}
