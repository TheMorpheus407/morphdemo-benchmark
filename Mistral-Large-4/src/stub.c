/*
 * stub.c — self-decompressing loader for morphdemo-4k.
 *
 * The 4 KiB executable is a tiny freestanding stub that carries the real
 * demo (a complete ELF) as a compressed payload in its .data section.
 * At startup the stub unpacks the payload into an anonymous in-memory
 * file (memfd), then execveat()s it — the unpacking is done entirely by
 * code inside this executable, as permitted by the spec.
 *
 * Payload stream (see tools/lzpack.py):
 *   [0..3]  little-endian uint32: decompressed length
 *   [4..]   range-coded LZSS item stream
 *
 * The range coder is an LZMA-style binary arithmetic coder with 25
 * adaptive 11-bit probability models:
 *   model 0            : item is a literal (1) or a match (0)
 *   models 1..8        : literal byte, bits MSB first
 *   models 9..9+OFFB   : match offset-1, OFFB bits MSB first
 *   models 9+OFFB..24  : match length-3, LENB bits MSB first
 * (OFFB + LENB = 16, so the LZSS token is always 2 bytes and the model
 * count is always 25, whatever the offset/length split.)
 * The encoder emits one leading dummy byte (its initial carry cache);
 * the decoder skips it.
 *
 * Payload symbols come from `ld -r -b binary` (see Makefile):
 *   _binary_payload_bin_start
 */

typedef unsigned long u64;
typedef long s64;

extern const unsigned char _binary_payload_bin_start[];

#define OFFB 12                        /* match offset bits: 1..4096 */
#define LENB 4                         /* match length bits: 3..18 */
#define NPROB (1 + 8 + OFFB + LENB)    /* 25 probability models */

/* noinline: called 4 times, one out-of-line copy beats four inlined
 * register-setup blocks */
__attribute__((noinline))
static long sys6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8  __asm__("r8")  = e;
    register long r9  __asm__("r9")  = f;
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return ret;
}

/* ---------------- range decoder ---------------- */

static unsigned rc_rng, rc_code;
static const unsigned char *rc_p;
static unsigned short rc_pr[NPROB];

static void rc_init(const unsigned char *p)
{
    rc_p = p + 1;                       /* skip the leading dummy byte */
    rc_rng = 0xFFFFFFFFu;
    rc_code = 0;
    for (int i = 0; i < 4; i++)
        rc_code = (rc_code << 8) | *rc_p++;
    for (int i = 0; i < NPROB; i++)
        rc_pr[i] = 1024;
}

static unsigned rc_bit(int ctx)
{
    unsigned bound = (rc_rng >> 11) * rc_pr[ctx];
    unsigned b;
    if (rc_code < bound) {
        rc_rng = bound;
        rc_pr[ctx] += (2048 - rc_pr[ctx]) >> 5;
        b = 0;
    } else {
        rc_code -= bound;
        rc_rng -= bound;
        rc_pr[ctx] -= rc_pr[ctx] >> 5;
        b = 1;
    }
    if (rc_rng < (1u << 24)) {
        rc_rng <<= 8;
        rc_code = (rc_code << 8) | *rc_p++;
    }
    return b;
}

/* read n bits, MSB first, from models base..base+n-1 */
static unsigned rc_bits(int base, int n)
{
    unsigned v = 0;
    for (int i = 0; i < n; i++)
        v = (v << 1) | rc_bit(base + i);
    return v;
}

/* ---------------- LZSS decoder ---------------- */

static void lzdec(unsigned char *dst, const unsigned char *src,
                  unsigned long outlen)
{
    rc_init(src);
    unsigned char *o = dst;
    while ((unsigned long)(o - dst) < outlen) {
        if (rc_bit(0)) {
            *o++ = (unsigned char)rc_bits(1, 8);
        } else {
            unsigned off = rc_bits(9, OFFB) + 1;
            unsigned len = rc_bits(9 + OFFB, LENB) + 3;
            unsigned char *p = o - off;
            while (len--)
                *o++ = *p++;
        }
    }
}

/* ---------------- stub ---------------- */

int stub_main(long argc, char **argv, char **envp)
{
    (void)argc;
    const unsigned char *ps = _binary_payload_bin_start;
    unsigned outlen;
    __builtin_memcpy(&outlen, ps, 4);

    long fd = sys6(319, (long)"", 0, 0, 0, 0, 0);             /* memfd_create */
    sys6(77, fd, (long)outlen, 0, 0, 0, 0);                  /* ftruncate */
    unsigned char *mem = (unsigned char *)
        sys6(9, 0, (long)outlen, 3, 0x01, fd, 0);            /* mmap RW SHARED */
    lzdec(mem, ps + 4, outlen);

    /* execveat(AT_EMPTY_PATH) on the memfd — no /proc path needed */
    sys6(322, fd, (long)"", (long)argv, (long)envp, 0x1000, 0);
    return 127;
}

__attribute__((noreturn)) void _start(void)
{
    __asm__ volatile(
        "xor %rbp, %rbp\n"
        "mov (%rsp), %rdi\n"            /* argc */
        "lea 8(%rsp), %rsi\n"           /* argv */
        "lea 16(%rsp,%rdi,8), %rdx\n"   /* envp */
        "and $-16, %rsp\n"
        "call stub_main\n"
        "mov %eax, %edi\n"
        "mov $231, %eax\n"      /* exit_group */
        "syscall\n");
    __builtin_unreachable();
}
