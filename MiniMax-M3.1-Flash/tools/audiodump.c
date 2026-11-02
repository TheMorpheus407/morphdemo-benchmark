/* audiodump.c -- render the soundtrack of a production into a WAV file.
 *
 * The three productions keep their synthesiser in a static function, so the
 * source is included here with main() renamed and the synthesiser is driven
 * directly.  The result is written as a 16 bit stereo WAV.
 *
 *   audiodump <demo16|demo64|demo4> <out.wav>
 */
#include <stdio.h>
#include <string.h>

#if defined(DEMO16)
#define main demo16_main
#include "../src/demo16.c"
#undef main
#define RATE 48000
#define DUR 60.0f
#elif defined(DEMO64)
#define main demo64_main
#include "../src/demo64.c"
#undef main
#define RATE 48000
#define DUR 60.0f
#elif defined(DEMO4)
#define main demo4_main
#include "../src/demo4.c"
#undef main
#define RATE 48000
#define DUR 60.0f
#endif

static void wr32(FILE *f, unsigned int v) {
    fputc(v & 255, f); fputc((v >> 8) & 255, f);
    fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f);
}
static void wr16(FILE *f, unsigned int v) {
    fputc(v & 255, f); fputc((v >> 8) & 255, f);
}

int main(int argc, char **argv) {
    FILE *f;
    int nframes = (int)(RATE * DUR);
    static float buf[4096 * 2];
    int done = 0;
    unsigned int data = 0;
    long i;

    if (argc < 3) { printf("usage: audiodump <demo16|demo64|demo4> <out.wav>\n"); return 1; }
    f = fopen(argv[2], "wb");
    if (!f) { perror("open"); return 1; }

    fwrite("RIFF", 1, 4, f);
    wr32(f, 36 + (unsigned)(nframes * 4));
    fwrite("WAVEfmt ", 1, 8, f);
    wr32(f, 16);
    wr16(f, 1); wr16(f, 2);
    wr32(f, RATE);
    wr32(f, RATE * 4);
    wr16(f, 4); wr16(f, 16);
    fwrite("data", 1, 4, f);
    wr32(f, (unsigned)(nframes * 4));

    while (done < nframes) {
        int n = nframes - done;
        if (n > 4096) n = 4096;
        synth(buf, n, (float)done / RATE);
        for (i = 0; i < n; i++) {
            float l = buf[2 * i], r = buf[2 * i + 1];
            if (l > 1.f) l = 1.f; if (l < -1.f) l = -1.f;
            if (r > 1.f) r = 1.f; if (r < -1.f) r = -1.f;
            wr16(f, (unsigned)(short)(l * 32000.f));
            wr16(f, (unsigned)(short)(r * 32000.f));
        }
        done += n;
    }
    fclose(f);
    printf("%s: %d frames written to %s\n", argv[1], nframes, argv[2]);
    return 0;
}
