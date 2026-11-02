/* stub.c -- the self extracting wrapper of the four kilobyte binary.
 *
 * The specification allows an executable to carry its own compressed code and
 * unpack it at runtime.  This file is that unpacker: it decompresses the
 * production that follows it in the binary into an anonymous in memory file
 * and executes that file, so the production itself runs as a normal program.
 *
 * Nothing outside the executable is needed for this.  The three system calls
 * that are used are made directly, the decompressor is a few dozen
 * instructions, and the file is written into a memfd that lives only as long
 * as the process does.
 */
typedef unsigned char u8;
typedef unsigned long ulong;

/* the compressed production, inserted by the assembler */
extern const u8 payload_start[];
extern const u8 payload_end[];

/* the unpacked production */
static u8 image[16384];

#define SYS_write 1
#define SYS_exit_group 231
#define SYS_memfd_create 319
#define SYS_execve 59

static long sys1(long n, long a) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}

static long sys2(long n, long a, long b) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b) : "rcx", "r11", "memory");
    return r;
}

static long sys3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c)
                      : "rcx", "r11", "memory");
    return r;
}

void cmain(long argc, char **argv, char **envp);

/* Unpack the payload into the image buffer.
 *
 * The format is a stream of items.  Every item starts with a token byte: the
 * high nibble counts the literal bytes that follow (15 means "read more"), the
 * low nibble holds the length of the match that follows the literals (minus
 * four).  The literals come first, then the distance, then the match length
 * extension if needed.  A distance below 255 is one byte; 255 means that two
 * more bytes carry the real distance.
 */
static ulong unpack(void) {
    const u8 *in = payload_start;
    u8 *out = image;

    while (in < payload_end) {
        unsigned token = *in++;
        unsigned lit = token >> 4;
        unsigned len, off;
        const u8 *src;

        if (lit == 15u) {
            unsigned s;
            do { s = *in++; lit += s; } while (s == 255u);
        }
        while (lit--) *out++ = *in++;
        if (in >= payload_end) break;

        off = in[0];
        in += 1;
        if (off == 255u) {                  /* the escape: two bytes follow */
            off = in[0] | (in[1] << 8);
            in += 2;
        }
        len = (token & 15u) + 4u;
        if ((token & 15u) == 15u) {
            unsigned s;
            do { s = *in++; len += s; } while (s == 255u);
        }
        src = out - off;
        while (len--) *out++ = *src++;
    }
    return (ulong)(out - image);
}

void cmain(long argc, char **argv, char **envp) {
    ulong n = unpack();
    long fd;
    char path[24];
    char *p = path;
    const char *pre = "/proc/self/fd/";
    char digits[24];
    int k = 0;
    long v;

    fd = sys2(SYS_memfd_create, (long)"morphdemo", 0);
    if (fd < 0) sys1(SYS_exit_group, 1);
    if (sys3(SYS_write, fd, (long)image, (long)n) != (long)n) sys1(SYS_exit_group, 1);

    /* The unpacked program lives in an anonymous in memory file.  It is
     * executed through the file descriptor, so nothing is written to the
     * file system and nothing is left behind. */
    while (*pre) *p++ = *pre++;
    v = fd;
    if (!v) digits[k++] = '0';
    while (v) { digits[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) *p++ = digits[--k];
    *p = 0;

    sys3(SYS_execve, (long)path, (long)argv, (long)envp);
    sys1(SYS_exit_group, 1);
}
