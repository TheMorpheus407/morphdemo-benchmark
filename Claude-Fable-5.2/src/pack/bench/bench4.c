#define main cmbench_main
#include "cmbench.c"
#undef main
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); data = malloc(len); if (fread(data, 1, len, f) != (size_t)len) return 1; fclose(f);
    nbits = len * 8; bitv = malloc(nbits); bpos = malloc(nbits); part = malloc(nbits);
    for (int i = 0, t = 0; i < len; i++) { int p = 1; for (int b = 7; b >= 0; b--, t++) { bitv[t] = data[i] >> b & 1; bpos[t] = 7 - b; part[t] = p; p = p * 2 + bitv[t]; } }
    int maxn = atoi(argv[2]);
    int rules[] = { 0, 2, 6, 7, 8 };
    for (int ri = 0; ri < 5; ri++) { upd_rule = rules[ri]; gen_all(0);
        for (int boost = 0; boost <= 1; boost++) for (int base = 1; base <= 8; base *= 2) {
            CrP p = { base, boost }; int ms[64], n = 0; double b = greedy(ev_cr, &p, ms, &n, maxn, 1, 0);
            printf("CR upd=%d boost=%d base=%d: %.1f bytes n=%d  masks:", upd_rule, boost, base, b / 8, n); for (int i = 0; i < n; i++) printf(" %02x/%d", ms[i], crw[i]); printf("\n"); fflush(stdout);
        } }
    return 0;
}
