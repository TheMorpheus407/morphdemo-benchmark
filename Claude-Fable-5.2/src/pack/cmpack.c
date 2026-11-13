// cmpack.c - context-mixing compressor for the MorphDemo self-extracting stubs ("CR" codec).
//
// Model (must mirror stub_cr.S bit for bit):
//   * N models, each an 8-bit mask selecting which of the previous 8 bytes form the context.
//   * hash = crc32c chain: seed = the little-endian dword that ends at the model's mask byte
//     in the file (so the mask is the top byte), then the selected history bytes, most recent
//     first, then the partial byte (leading 1 + bits decoded so far).  slot = top HBITS bits.
//     A slot is 2 bytes: n0, n1; counters wrap modulo 256.
//   * prediction: n0 = BASE + sum(n0_i << (c>>1)), n1 likewise, where c counts down over the
//     model list (c = N-1+CSTART .. CSTART); threshold = range*n0/(n0+n1).
//     Bitwise binary arithmetic decoder (32-bit range, 1 bit per normalisation step).
//   * update: n[bit]++ ; n[!bit] = (n+1)>>1 (UPD=2) or n>1 ? n>>1 : n (UPD=0, Crinkler rule).
//
// The tool searches the model list for a payload (greedy insert/remove/swap on an ordered
// list, evaluated on collision-free "ideal" counter streams), then encodes with the exact
// hashed table and verifies the round trip with the reference decoder below.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#ifdef _OPENMP
#include <omp.h>
#endif

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#define MAXM 32
#define NMASK 256

typedef struct {
    int n; u8 masks[MAXM];
    int hbits;    // log2(number of slots)
    int base;     // base count added to both sides
    int upd;      // 0: Crinkler rule, 2: (n+1)>>1
    int cstart;   // 0: counter N-1..0, 1: counter N..1
    u8 prefix[3]; // the three file bytes preceding the model list (part of the per-model hash seeds)
} Params;

static void die(const char *m) { fprintf(stderr, "cmpack: %s\n", m); exit(2); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ---------------------------------------------------------------- crc32c (matches the x86 crc32 instruction)
static u32 crctab[256];
static void crc_init(void) { for (u32 i = 0; i < 256; i++) { u32 c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1))); crctab[i] = c; } }
static inline u32 crc8(u32 h, u8 b) { return crctab[(h ^ b) & 255] ^ (h >> 8); }

// ---------------------------------------------------------------- exact model
typedef struct {
    Params p; u8 *tab; u64 nslots; u8 *slot[MAXM];
} Model;
static void model_init(Model *m, const Params *p) {
    m->p = *p; m->nslots = 1ull << p->hbits; m->tab = calloc(m->nslots, 2); if (!m->tab) die("out of memory (hash table)");
}
static void model_free(Model *m) { free(m->tab); }
// hist points at the byte being decoded; hist[-1..-8] must be readable (zero before the start)
static inline void model_predict(Model *m, const u8 *hist, u32 partial, u32 *n0, u32 *tot) {
    const Params *p = &m->p; u32 a0 = (u32)p->base, a1 = (u32)p->base;
    int c = p->n - 1 + p->cstart;
    for (int i = 0; i < p->n; i++, c--) {
        // seed = little-endian dword ending at the mask byte: [m(i-3) m(i-2) m(i-1) m(i)], the list being
        // preceded by p->prefix.  Mask in the top byte keeps different models from aliasing.
        u8 mask = p->masks[i]; u32 h = (u32)mask << 24;
        for (int j = 1; j <= 3; j++) { int k = i - j; u8 v = k >= 0 ? p->masks[k] : p->prefix[3 + k]; h |= (u32)v << (24 - 8 * j); }
        const u8 *q = hist; u8 mm = mask;
        do { q--; int b = mm & 1; mm >>= 1; if (b) h = crc8(h, *q); } while (mm);
        h = crc8(h, (u8)partial);
        u8 *s = m->tab + (u64)(h >> (32 - p->hbits)) * 2; m->slot[i] = s;
        a0 += (u32)s[0] << (c >> 1); a1 += (u32)s[1] << (c >> 1);
    }
    *n0 = a0; *tot = a0 + a1;
}
static inline void model_update(Model *m, int bit) {
    const Params *p = &m->p;
    for (int i = p->n - 1; i >= 0; i--) {
        u8 *a = m->slot[i] + bit, *b = m->slot[i] + (1 - bit);
        (*a)++;
        if (p->upd == 2) *b = (u8)((u8)(*b + 1) >> 1); else if (*b > 1) *b >>= 1;
    }
}

// ---------------------------------------------------------------- arithmetic coder
// Encoder state mirrors the decoder: R is the decoder's range, L the low end (big number kept as
// frozen bits + 32-bit window).  The stream is the bit string of a value in [L, L+R).
typedef struct { u8 *bits; size_t nb, cap; u32 W, R; int k; } Enc;
static void enc_carry(Enc *e) { size_t i = e->nb; while (i > 0) { i--; if (e->bits[i]) e->bits[i] = 0; else { e->bits[i] = 1; return; } } die("arithmetic coder carry overflow"); }
static void enc_shift(Enc *e) {
    int top = e->W >> 31; e->W <<= 1;
    if (e->k >= 32) { if (e->nb == e->cap) { e->cap = e->cap * 2 + 1024; e->bits = realloc(e->bits, e->cap); } e->bits[e->nb++] = top; }
    else if (top) die("arithmetic coder internal error");
    e->k++;
}
static void enc_init(Enc *e) { memset(e, 0, sizeof *e); e->R = 1; for (int i = 0; i < 31; i++) { e->R <<= 1; enc_shift(e); } }
static void enc_bit(Enc *e, int bit, u32 n0, u32 tot) {
    while (!(e->R & 0x80000000u)) { e->R <<= 1; enc_shift(e); }
    u32 t = (u32)(((u64)e->R * n0) / tot);
    if (bit) { u32 w = e->W + t; if (w < e->W) enc_carry(e); e->W = w; e->R -= t; } else e->R = t;
}
// returns byte array (bit i of the stream is bit (i&7) of byte i>>3); *nbits = meaningful bits.
// If trim is set, trailing zero bits are dropped (valid only if the decoder will read zeros there).
static u8 *enc_finish(Enc *e, size_t *nbytes, int trim) {
    // choose V in [L, L+R-1] with as many trailing zero bits as possible
    u64 hi = (u64)e->W + e->R - 1;           // may exceed 32 bits -> carry
    int j; u64 V = hi;
    for (j = 33; j >= 0; j--) { u64 cand = j >= 34 ? 0 : (hi >> j) << j; if (cand >= e->W) { V = cand; break; } }
    if (V >> 32) { enc_carry(e); V &= 0xffffffffu; }
    int ww = e->k >= 32 ? 32 : e->k;            // width of the live window in bits
    size_t total = e->nb + ww; u8 *all = malloc(total + 8);
    memcpy(all, e->bits, e->nb); for (int i = 0; i < ww; i++) all[e->nb + i] = (V >> (ww - 1 - i)) & 1;
    // the decoder consumes exactly k bits (frozen bits + window); bits beyond k are never read
    size_t used = (size_t)e->k; if (used != total) die("coder accounting");
    if (trim) while (used > 0 && all[used - 1] == 0) used--;
    size_t nby = (used + 7) / 8; u8 *out = calloc(nby + 8, 1);
    for (size_t i = 0; i < used; i++) if (all[i]) out[i >> 3] |= 1 << (i & 7);
    free(all); *nbytes = nby; return out;
}
// reference decoder: mirrors the stub.  src may be read past the stream (caller pads with the real following bytes)
static int ref_decode(const Params *p, const u8 *src, size_t srcbits_avail, u8 *out, int len) {
    Model m; model_init(&m, p);
    u8 *buf = calloc(len + 16, 1); u8 *o = buf + 16;
    u32 range = 1, value = 0; u64 bp = 0; int ok = 1;
    for (int pos = 0; pos < len && ok; pos++) {
        u32 partial = 1;
        for (int b = 0; b < 8; b++) {
            u32 n0, tot; model_predict(&m, o + pos, partial, &n0, &tot);
            while (!(range & 0x80000000u)) { int bit = 0; if (bp < srcbits_avail) bit = src[bp >> 3] >> (bp & 7) & 1; bp++; range <<= 1; value = (value << 1) | bit; }
            u32 t = (u32)(((u64)range * n0) / tot); int bit;
            if (value < t) { bit = 0; range = t; } else { bit = 1; value -= t; range -= t; }
            model_update(&m, bit); partial = partial * 2 + bit;
        }
        o[pos] = (u8)partial;
    }
    memcpy(out, o, len); free(buf); model_free(&m); return ok;
}
static u8 *encode(const Params *p, const u8 *data, int len, size_t *nbytes, int trim, double *cost_bits, double *bytecost) {
    Model m; model_init(&m, p); Enc e; enc_init(&e);
    u8 *buf = calloc(len + 16, 1); u8 *o = buf + 16; memcpy(o, data, len); double cost = 0;
    for (int pos = 0; pos < len; pos++) {
        u32 partial = 1; double bc = 0;
        for (int b = 7; b >= 0; b--) {
            int bit = data[pos] >> b & 1; u32 n0, tot; model_predict(&m, o + pos, partial, &n0, &tot);
            if (n0 == 0 || n0 >= tot) die("degenerate probability (base must be >= 1)");
            enc_bit(&e, bit, n0, tot);
            bc += log2((double)tot / (bit ? tot - n0 : n0));
            model_update(&m, bit); partial = partial * 2 + bit;
        }
        cost += bc; if (bytecost) bytecost[pos] = bc;
    }
    if (cost_bits) *cost_bits = cost;
    u8 *out = enc_finish(&e, nbytes, trim); free(e.bits); free(buf); model_free(&m); return out;
}

// ---------------------------------------------------------------- ideal (collision-free) counter streams for the search
static const u8 *g_data; static int g_len, g_nbits; static u8 *g_bitv; static u8 *g_cnt[NMASK];
static int g_step = 1;      // evaluate every g_step-th byte during search
static inline u64 mix64(u64 x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return x; }
typedef struct { u64 key; u32 idx; } HE;
static void gen_mask(int m, int upd) {
    int cap = 1; while (cap < g_nbits * 2) cap <<= 1;
    HE *ht = calloc(cap, sizeof(HE)); u8 *st = calloc((size_t)g_nbits + 1, 2); u32 nst = 1;
    u8 *out = g_cnt[m] = malloc(2 * (size_t)g_nbits); if (!ht || !st || !out) die("out of memory (streams)");
    int t = 0;
    for (int pos = 0; pos < g_len; pos++) {
        u64 ch = 0x9E3779B97F4A7C15ULL * (m + 1);
        for (int j = 0; j < 8; j++) if (m >> j & 1) { u8 c = pos - 1 - j >= 0 ? g_data[pos - 1 - j] : 0; ch = mix64(ch ^ (c + 1) ^ ((u64)j << 32)); }
        int partial = 1;
        for (int b = 7; b >= 0; b--, t++) {
            u64 key = mix64(ch ^ ((u64)partial << 40)) | 1; u32 h = (u32)(key >> 20) & (cap - 1);
            while (ht[h].key && ht[h].key != key) h = (h + 1) & (cap - 1);
            if (!ht[h].key) { ht[h].key = key; ht[h].idx = nst++; }
            u8 *s = st + 2 * ht[h].idx; out[2 * t] = s[0]; out[2 * t + 1] = s[1];
            int bit = g_data[pos] >> b & 1;
            s[bit]++;
            if (upd == 2) s[!bit] = (u8)((u8)(s[!bit] + 1) >> 1); else if (s[!bit] > 1) s[!bit] >>= 1;
            partial = partial * 2 + bit;
        }
    }
    free(ht); free(st);
}
static void gen_all(int upd) {
    #pragma omp parallel for schedule(dynamic)
    for (int m = 0; m < NMASK; m++) gen_mask(m, upd);
}
static void free_all(void) { for (int m = 0; m < NMASK; m++) { free(g_cnt[m]); g_cnt[m] = 0; } }
// cost in bits of an ordered model list under the positional schedule (ideal counters)
static double eval_list(const u8 *ms, int n, int base, int cstart) {
    int w[MAXM]; const u8 *cp[MAXM];
    for (int i = 0; i < n; i++) { w[i] = (n - 1 - i + cstart) >> 1; cp[i] = g_cnt[ms[i]]; }
    double bits = 0, prod = 1.0; int cntr = 0;
    for (int pos = 0; pos < g_len; pos += g_step) {
        for (int t = pos * 8; t < pos * 8 + 8; t++) {
            u64 n0 = base, n1 = base;
            for (int i = 0; i < n; i++) { const u8 *c = cp[i] + 2 * (size_t)t; n0 += (u64)c[0] << w[i]; n1 += (u64)c[1] << w[i]; }
            prod *= (double)(n0 + n1) / (double)(g_bitv[t] ? n1 : n0);
            if (++cntr == 16) { int ex; prod = frexp(prod, &ex); bits += ex; cntr = 0; }
        }
    }
    return (bits + log2(prod)) * g_step;
}
static double search(Params *p, int maxn, int verbose) {
    u8 ms[MAXM]; int n = p->n; memcpy(ms, p->masks, n);
    double best = n ? eval_list(ms, n, p->base, p->cstart) : 1e300;
    for (int round = 0; round < 200; round++) {
        int improved = 0;
        if (n < maxn) {                                  // insertion of the best (mask, position)
            double cb[NMASK]; int cpn[NMASK];
            #pragma omp parallel for schedule(dynamic)
            for (int m = 0; m < NMASK; m++) {
                cb[m] = 1e300; cpn[m] = 0; int dup = 0; for (int i = 0; i < n; i++) if (ms[i] == m) dup = 1;
                if (dup) continue;
                for (int q = 0; q <= n; q++) { u8 tmp[MAXM]; memcpy(tmp, ms, q); tmp[q] = m; memcpy(tmp + q + 1, ms + q, n - q);
                    double b = eval_list(tmp, n + 1, p->base, p->cstart); if (b < cb[m]) { cb[m] = b; cpn[m] = q; } }
            }
            int bm = -1; double bb = best - 8.0;         // a model costs one byte
            for (int m = 0; m < NMASK; m++) if (cb[m] < bb) { bb = cb[m]; bm = m; }
            if (bm >= 0) { int q = cpn[bm]; memmove(ms + q + 1, ms + q, n - q); ms[q] = bm; n++; best = bb; improved = 1;
                if (verbose) fprintf(stderr, "  + %02x @%d -> %.1f bytes (n=%d)\n", bm, q, best / 8, n); }
        }
        for (int i = 0; i < n && n > 1; i++) {           // removal
            u8 tmp[MAXM]; int k = 0; for (int j = 0; j < n; j++) if (j != i) tmp[k++] = ms[j];
            double b = eval_list(tmp, k, p->base, p->cstart);
            if (b < best + 8.0 - 0.01) { if (verbose) fprintf(stderr, "  - %02x -> %.1f bytes\n", ms[i], b / 8); memcpy(ms, tmp, k); n = k; best = b; improved = 1; i--; }
        }
        for (int i = 0; i + 1 < n; i++) {                // adjacent swaps
            u8 t = ms[i]; ms[i] = ms[i + 1]; ms[i + 1] = t; double b = eval_list(ms, n, p->base, p->cstart);
            if (b < best - 0.5) { best = b; improved = 1; } else { ms[i + 1] = ms[i]; ms[i] = t; }
        }
        if (round > 0 || n == maxn) {                    // replacement (expensive; only once the list is populated)
            for (int i = 0; i < n; i++) {
                double cb[NMASK];
                #pragma omp parallel for schedule(dynamic)
                for (int m = 0; m < NMASK; m++) { cb[m] = 1e300; int dup = 0; for (int j = 0; j < n; j++) if (ms[j] == m) dup = 1; if (dup) continue;
                    u8 tmp[MAXM]; memcpy(tmp, ms, n); tmp[i] = m; cb[m] = eval_list(tmp, n, p->base, p->cstart); }
                int bm = -1; double bb = best - 0.5; for (int m = 0; m < NMASK; m++) if (cb[m] < bb) { bb = cb[m]; bm = m; }
                if (bm >= 0) { if (verbose) fprintf(stderr, "  ~ %02x->%02x @%d -> %.1f bytes\n", ms[i], bm, i, bb / 8); ms[i] = bm; best = bb; improved = 1; }
            }
        }
        if (!improved) break;
    }
    p->n = n; memcpy(p->masks, ms, n); return best;
}

// ---------------------------------------------------------------- driver
static u8 *readfile(const char *fn, int *len) {
    FILE *f = fopen(fn, "rb"); if (!f) { perror(fn); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); u8 *d = malloc(n + 1); if (fread(d, 1, n, f) != (size_t)n) die("read error"); fclose(f); *len = (int)n; return d;
}
static void writefile(const char *fn, const void *d, size_t n) { FILE *f = fopen(fn, "wb"); if (!f) { perror(fn); exit(2); } fwrite(d, 1, n, f); fclose(f); }
static void usage(void) {
    fprintf(stderr,
        "usage: cmpack [options] payload\n"
        "  -o FILE     write compressed stream\n"
        "  -m FILE     write model list (decoder order)\n"
        "  -M HEX      use this model list (hex bytes, decoder order) instead of / as start for searching\n"
        "  -S          search the model list (greedy), otherwise just encode with -M\n"
        "  -n N        maximum number of models (default 16)\n"
        "  -k BITS     hash table slots = 2^BITS (default 24)\n"
        "  -b BASE     base count (default 0 = try 1,2,3,4,6 when searching)\n"
        "  -u UPD      counter update rule 0 or 2 (default -1 = try both when searching)\n"
        "  -c CSTART   weight schedule offset 0 or 1 (default -1 = try both when searching)\n"
        "  -s STEP     search on every STEP-th byte (default: auto)\n"
        "  -P HEX      the 3 bytes preceding the model list in the final file (default 000f05)\n"
        "  -T          do not trim trailing zero bits of the stream\n"
        "  -B FILE     write per-byte cost table (double per payload byte)\n"
        "  -d FILE     decode test: decode -o stream again with the reference decoder and compare\n"
        "  -v          verbose\n");
    exit(2);
}
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static int parse_hex(const char *s, u8 *out) {
    int n = 0;
    while (*s) { if (*s == ' ' || *s == ',') { s++; continue; } int a = hexv(s[0]), b = a >= 0 ? hexv(s[1]) : -1; if (a < 0 || b < 0 || n >= MAXM) die("bad hex list"); out[n++] = (u8)(a * 16 + b); s += 2; }
    return n;
}
int main(int argc, char **argv) {
    const char *ofn = 0, *mfn = 0, *bfn = 0, *payload = 0; Params p; memset(&p, 0, sizeof p); p.hbits = 24; p.base = 0; p.upd = -1; p.cstart = -1;
    p.prefix[0] = 0x00; p.prefix[1] = 0x0f; p.prefix[2] = 0x05;
    int do_search = 0, maxn = 16, verbose = 0, trim = 1, step = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') { payload = argv[i]; continue; }
        switch (argv[i][1]) {
        case 'o': ofn = argv[++i]; break; case 'm': mfn = argv[++i]; break; case 'M': p.n = parse_hex(argv[++i], p.masks); break;
        case 'S': do_search = 1; break; case 'n': maxn = atoi(argv[++i]); break; case 'k': p.hbits = atoi(argv[++i]); break;
        case 'b': p.base = atoi(argv[++i]); break; case 'u': p.upd = atoi(argv[++i]); break; case 'c': p.cstart = atoi(argv[++i]); break;
        case 'P': { u8 t[MAXM]; if (parse_hex(argv[++i], t) != 3) die("-P needs 3 bytes"); memcpy(p.prefix, t, 3); } break;
        case 's': step = atoi(argv[++i]); break; case 'T': trim = 0; break; case 'B': bfn = argv[++i]; break; case 'v': verbose = 1; break;
        default: usage();
        }
    }
    if (!payload) usage();
    if (maxn > MAXM) maxn = MAXM; if (p.hbits < 12 || p.hbits > 30) die("-k must be 12..30");
    crc_init();
    int len; u8 *data = readfile(payload, &len); if (len < 1) die("empty payload");
    double t0 = now();
    if (do_search) {
        g_data = data; g_len = len; g_nbits = len * 8; g_bitv = malloc(g_nbits);
        for (int i = 0, t = 0; i < len; i++) for (int b = 7; b >= 0; b--, t++) g_bitv[t] = data[i] >> b & 1;
        g_step = step > 0 ? step : (len > 24000 ? (len + 11999) / 12000 : 1);
        int upds[2] = { 0, 2 }, nu = 2; if (p.upd >= 0) { upds[0] = p.upd; nu = 1; }
        int cs[2] = { 1, 0 }, nc = 2; if (p.cstart >= 0) { cs[0] = p.cstart; nc = 1; }
        int bases[5] = { 2, 3, 4, 1, 6 }, nb = 5; if (p.base > 0) { bases[0] = p.base; nb = 1; }
        Params best = p; double bestc = 1e300; int have = 0;
        for (int ui = 0; ui < nu; ui++) {
            gen_all(upds[ui]);
            // stage 1: search with the first base/cstart, then refine the other parameters on the found list
            for (int ci = 0; ci < nc; ci++) for (int bi = 0; bi < nb; bi++) {
                Params q = p; q.upd = upds[ui]; q.cstart = cs[ci]; q.base = bases[bi];
                if (bi > 0 && have) { q.n = best.n; memcpy(q.masks, best.masks, best.n); }   // warm start from the best list so far
                double c = search(&q, maxn, verbose) + 8.0 * q.n + (q.upd == 0 ? 24.0 : 0.0);  // UPD=0 costs 3 more stub bytes
                if (verbose) fprintf(stderr, "upd=%d cstart=%d base=%d: n=%d est %.1f bytes (%.1fs)\n", q.upd, q.cstart, q.base, q.n, c / 8, now() - t0);
                if (c < bestc) { bestc = c; best = q; have = 1; }
            }
            free_all();
        }
        p = best;
    } else {
        if (p.n == 0) die("no model list given (-M) and no search requested (-S)");
        if (p.upd < 0) p.upd = 2; if (p.cstart < 0) p.cstart = 1; if (p.base <= 0) p.base = 2;
    }
    // exact encode + verify
    size_t nby; double cost; double *bytecost = bfn ? calloc(len, sizeof(double)) : 0;
    u8 *out = encode(&p, data, len, &nby, trim, &cost, bytecost);
    u8 *chk = malloc(len + 1); ref_decode(&p, out, nby * 8, chk, len);
    if (memcmp(chk, data, len)) die("ROUND TRIP FAILED (reference decoder mismatch)");
    if (ofn) writefile(ofn, out, nby);
    if (mfn) writefile(mfn, p.masks, p.n);
    if (bfn) writefile(bfn, bytecost, len * sizeof(double));
    printf("payload=%d stream=%zu models=%d hbits=%d base=%d upd=%d cstart=%d ideal_bits=%.0f masks=", len, nby, p.n, p.hbits, p.base, p.upd, p.cstart, cost);
    for (int i = 0; i < p.n; i++) printf("%02x", p.masks[i]);
    printf(" time=%.2f\n", now() - t0);
    return 0;
}
