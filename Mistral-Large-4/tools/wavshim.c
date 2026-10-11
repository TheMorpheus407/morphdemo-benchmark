/*
 * wavshim.c — LD_PRELOAD debug shim for morphdemo TEST builds only.
 *
 * Intercepts two PLT calls of the demo binary:
 *   pa_simple_write  -> appends the exact s16le stereo bytes the demo hands
 *                       to PulseAudio to build/dbg_audio.wav (44-byte WAV
 *                       header written first, sizes patched afterwards) and
 *                       logs write progress to build/aud.log.
 *   glXSwapBuffers   -> counts presented frames and logs the frame rate to
 *                       build/fps.log every 120 frames.
 *
 * Build (host toolchain, NOT the freestanding one):
 *   gcc -shared -fPIC -O2 -o build/wavshim.so tools/wavshim.c -ldl
 * Use:
 *   DISPLAY=:0 LD_PRELOAD=$PWD/build/wavshim.so ./build/t32
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

static double monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void log_line(const char *path, const char *fmt, ...)
{
    int lf = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (lf < 0) return;
    va_list ap;
    va_start(ap, fmt);
    vdprintf(lf, fmt, ap);
    va_end(ap);
    close(lf);
}

static void put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static void put_u16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}

static void wav_header(unsigned char *h, unsigned long data_bytes)
{
    memcpy(h, "RIFF", 4);
    put_u32(h + 4, 36 + data_bytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    put_u32(h + 16, 16);            /* fmt chunk size   */
    put_u16(h + 20, 1);             /* PCM              */
    put_u16(h + 22, 2);             /* stereo           */
    put_u32(h + 24, 48000);         /* rate             */
    put_u32(h + 28, 48000 * 4);     /* byte rate        */
    put_u16(h + 32, 4);             /* block align      */
    put_u16(h + 34, 16);            /* bits per sample  */
    memcpy(h + 36, "data", 4);
    put_u32(h + 40, data_bytes);
}

typedef struct _pa_simple pa_simple;

int pa_simple_write(pa_simple *s, const void *data, size_t bytes, int *error)
{
    static int (*real_fn)(pa_simple *, const void *, size_t, int *);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "pa_simple_write");

    static int fd = -1;
    static unsigned long long total = 0;
    static int calls = 0;
    static double t0 = 0.0;

    if (fd < 0) {
        t0 = monotonic();
        unsigned char hdr[44];
        wav_header(hdr, 0xffffffffUL);       /* patched after the run */
        fd = open("build/dbg_audio.wav", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            if (write(fd, hdr, 44) != 44) { /* ignore */ }
            int lf = open("build/aud.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (lf >= 0) {
                dprintf(lf, "t=0.000 first audio write\n");
                close(lf);
            }
        }
    }
    if (fd >= 0) {
        ssize_t w = write(fd, data, bytes);
        if (w > 0) total += (unsigned long long)w;
    }
    calls++;
    {
        double tw0 = monotonic();
        int rr = real_fn(s, data, bytes, error);
        log_line("build/aud.log", "t=%.3f bytes=%llu calls=%d wt=%.4f\n",
                 monotonic() - t0, total, calls, monotonic() - tw0);
        return rr;
    }
}

typedef struct _XDisplay Display;
typedef unsigned long GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;

static void log_gl_strings(void)
{
    static const char *(*get_string)(unsigned int);
    if (!get_string) get_string = dlsym(RTLD_NEXT, "glGetString");
    static int (*is_direct)(Display *, GLXContext);
    if (!is_direct) is_direct = dlsym(RTLD_NEXT, "glXIsDirect");
    static int done = 0;
    if (done || !get_string) return;
    done = 1;
    log_line("build/gl.log", "GL_VENDOR=%s\n", (const char *)get_string(0x1F00));
    log_line("build/gl.log", "GL_RENDERER=%s\n", (const char *)get_string(0x1F01));
    log_line("build/gl.log", "GL_VERSION=%s\n", (const char *)get_string(0x1F02));
    log_line("build/gl.log", "GLSL_VERSION=%s\n", (const char *)get_string(0x8B8C));
    if (is_direct)
        log_line("build/gl.log", "glXIsDirect=%d\n", is_direct(0, 0));
}

int glXMakeCurrent(Display *d, GLXDrawable dr, GLXContext ctx)
{
    static int (*real_fn)(Display *, GLXDrawable, GLXContext);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "glXMakeCurrent");
    int r = real_fn(d, dr, ctx);
    if (r) log_gl_strings();
    return r;
}

GLXContext glXCreateContext(Display *d, void *vi, GLXContext share, int direct)
{
    static GLXContext (*real_fn)(Display *, void *, GLXContext, int);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "glXCreateContext");
    double t0 = monotonic();
    GLXContext r = real_fn(d, vi, share, direct);
    log_line("build/gl.log", "glXCreateContext(direct=%d)=%p took %.3fs\n",
             direct, (void *)r, monotonic() - t0);
    return r;
}

void glCompileShader(unsigned int sh)
{
    static void (*real_fn)(unsigned int);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "glCompileShader");
    double t0 = monotonic();
    real_fn(sh);
    log_line("build/gl.log", "glCompileShader took %.3fs\n", monotonic() - t0);
}

void glEnd(void)
{
    static void (*real_fn)(void);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "glEnd");
    static int n = 0;
    static double t0 = 0.0, tlast = 0.0;
    if (n == 0) { t0 = monotonic(); tlast = t0; }
    n++;
    double t = monotonic();
    log_line("build/fps.log", "glend n=%d t=%.3f dt=%.3f\n", n, t - t0, t - tlast);
    tlast = t;
    real_fn();
}

void glXSwapBuffers(Display *d, GLXDrawable drawable)
{
    static void (*real_fn)(Display *, GLXDrawable);
    if (!real_fn) real_fn = dlsym(RTLD_NEXT, "glXSwapBuffers");

    real_fn(d, drawable);

    static int frames = 0;
    static double t0 = 0.0, tlast = 0.0;
    if (frames == 0) { t0 = monotonic(); tlast = t0; }
    frames++;
    double t = monotonic();
    log_line("build/fps.log", "swap n=%d t=%.3f dt=%.3f\n", frames, t - t0, t - tlast);
    tlast = t;
}
