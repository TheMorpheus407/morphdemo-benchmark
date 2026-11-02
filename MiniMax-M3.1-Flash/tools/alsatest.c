/* alsatest.c -- check that the ALSA default device can be opened and written.
 *
 * Usage: alsatest [seconds]
 */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

int main(int argc, char **argv) {
    int (*snd_pcm_open)(void **, const char *, int, int) = dlsym(RTLD_DEFAULT, "snd_pcm_open");
    int (*snd_pcm_set_params)(void *, int, int, unsigned int, unsigned int, int, unsigned int) =
        dlsym(RTLD_DEFAULT, "snd_pcm_set_params");
    long (*snd_pcm_writei)(void *, const void *, unsigned long) = dlsym(RTLD_DEFAULT, "snd_pcm_writei");
    const char *(*snd_pcm_strerror)(int) = dlsym(RTLD_DEFAULT, "snd_pcm_strerror");
    void *h = 0;
    int err;
    float buf[2048];
    long w, total = 0;
    int i, n = argc > 1 ? atoi(argv[1]) : 1;

    for (i = 0; i < 2048; i++) buf[i] = 0.0f;
    err = snd_pcm_open(&h, "default", 0 /* playback */, 0);
    if (err < 0) { printf("open failed: %s\n", snd_pcm_strerror(err)); return 1; }
    err = snd_pcm_set_params(h, 14 /* FLOAT_LE */, 3 /* RW_INTERLEAVED */, 2, 48000, 1, 200000);
    if (err < 0) { printf("set_params failed: %s\n", snd_pcm_strerror(err)); return 1; }
    for (i = 0; i < n * 48000 / 1024; i++) {
        w = snd_pcm_writei(h, buf, 1024);
        if (w < 0) { printf("write failed: %s\n", snd_pcm_strerror((int)w)); return 1; }
        total += w;
    }
    printf("ALSA ok, wrote %ld frames\n", total);
    return 0;
}
