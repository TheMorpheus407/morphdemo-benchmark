/* alsarate.c -- how fast does the sound device really consume?
 *
 * Writes blocks of frames and measures the wall clock time it takes to
 * push one second of audio through the device.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>

typedef struct snd_pcm_t snd_pcm_t;
typedef struct snd_pcm_hw_params_t snd_pcm_hw_params_t;

extern int snd_pcm_open(snd_pcm_t **, const char *, int, int);
extern int snd_pcm_set_params(snd_pcm_t *, int, int, unsigned int, unsigned int,
                               int, unsigned int);
extern long snd_pcm_writei(snd_pcm_t *, const void *, unsigned long);
extern int snd_pcm_close(snd_pcm_t *);

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    snd_pcm_t *pcm = NULL;
    static float buf[4096 * 2];
    int rate = argc > 1 ? atoi(argv[1]) : 48000;
    int blocks = rate / 1024;
    double t0, t1;
    long total = 0;
    int i;

    for (i = 0; i < 4096 * 2; i++) buf[i] = 0.f;

    if (snd_pcm_open(&pcm, "default", 0, 0) < 0) { printf("open failed\n"); return 1; }
    if (snd_pcm_set_params(pcm, 14, 3, 2, (unsigned)rate, 1, 200000) < 0) {
        printf("set_params failed\n"); return 1;
    }
    t0 = now();
    for (i = 0; i < blocks; i++) {
        long n = snd_pcm_writei(pcm, buf, 1024);
        if (n < 0) { printf("write error %ld at block %d\n", n, i); break; }
        total += n;
    }
    t1 = now();
    printf("%d blocks of 1024 = %ld frames in %.3f s -> %.1f frames/s (asked %d)\n",
           blocks, total, t1 - t0, (double)total / (t1 - t0), rate);
    printf("ratio device/wall = %.4f\n", (double)total / (rate * (t1 - t0)));
    snd_pcm_close(pcm);
    return 0;
}
