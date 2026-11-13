// Round 3: CR vs LG greedy at larger scales with small grids (bench only).
#define main cmbench_main
#include "cmbench.c"
#undef main
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    int maxn = atoi(argv[2]); const char *what = argv[3];
    upd_rule = 0; gen_all(0);
    if (strchr(what, 'c')) for (int base = 2; base <= 4; base += 2) { CrP p = { base, 1 }; int ms[64], n = 0; double b = greedy(ev_cr, &p, ms, &n, maxn, 1, 0);
        printf("CR boost=1 base=%d: %.1f bytes n=%d masks:", base, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x/%d", ms[i], crw[i]); printf("\n"); fflush(stdout); }
    if (strchr(what, 'l')) for (int kind = 0; kind <= 1; kind++) for (int sel = 0; sel <= 2; sel++) {
        LgP p = { kind, 0.1f, kind ? 0.0007f : 0.002f, kind ? 0.1f : 0.15f, sel, 0, 0 }; int ms[64], n = 0; double b = greedy(ev_lg, &p, ms, &n, maxn, 0, 0);
        printf("LG kind=%d sel=%d: %.1f bytes n=%d masks:", kind, sel, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x", ms[i]); printf("\n");
        // retune lr/delta on the found set
        double bb = 1e30; LgP bp = p; for (int di = 0; di < 3; di++) for (int li = 0; li < 4; li++) { LgP q = p; q.delta = 0.05f * (1 << di); q.lr = p.lr * 0.5f * (1 << li); double v = eval_lg(ms, n, q); if (v < bb) { bb = v; bp = q; } }
        printf("    retuned: %.1f (delta %.2f lr %.4f)\n", bb / 8, bp.delta, bp.lr); fflush(stdout);
    }
    return 0;
}
