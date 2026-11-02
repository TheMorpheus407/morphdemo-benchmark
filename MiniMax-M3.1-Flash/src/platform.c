/* platform.c -- window, context, sound device and clock.
 *
 * Only libraries that are already present in the supplied environment are
 * used: libX11 with libGL for the window and the context, and libasound for
 * the sound device.  Nothing is read from disk at run time.
 *
 * The OpenGL context is created the classic way, with glXChooseVisual and
 * glXCreateContext.  The driver answers that request with a 4.6
 * compatibility profile, which is enough for the shaders used here and works
 * the same way on the software rasteriser and on the real card.
 */
#define MORPH_SYMTAB_DEFINE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"
#include "syms_common.h"

#define SYMS(X) SYMS_ALL(X)
#include "symtab.h"

Display   *g_dpy;
Window     g_win;
GLXContext g_ctx;
int        g_width  = 1920;
int        g_height = 1080;
int        g_rate   = 48000;

static void *g_pcm;                 /* the ALSA handle, NULL when no device */
static double g_t0;                 /* wall clock of plat_init()          */
static GLuint g_prog;               /* the program in use                 */

/* ------------------------------------------------------------- timing ---- */
double plat_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ------------------------------------------------------- display setup --- */
int plat_init(int w, int h, const char *title) {
    static const int vattrs[] = {
        GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };
    XSetWindowAttributes swa;
    XSizeHints hints;
    XVisualInfo *vi;
    Colormap cmap;
    Window root, win;
    int scr;

    sym_load();

    g_dpy = SYM(XOpenDisplay, NULL);
    if (!g_dpy) {
        fprintf(stderr, "morphdemo: cannot open the display (DISPLAY=%s)\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "unset");
        return 0;
    }
    scr = SYM(XDefaultScreen, g_dpy);
    root = SYM(XDefaultRootWindow, g_dpy);

    /* never larger than the screen */
    {
        int sw = SYM(XDisplayWidth, g_dpy, scr);
        int sh = SYM(XDisplayHeight, g_dpy, scr);
        if (sw > 0 && w > sw) w = sw;
        if (sh > 0 && h > sh) h = sh;
    }
    g_width = w;
    g_height = h;

    vi = SYM(glXChooseVisual, g_dpy, scr, (int *)vattrs);
    if (!vi) {
        fprintf(stderr, "morphdemo: no usable visual\n");
        return 0;
    }
    cmap = SYM(XCreateColormap, g_dpy, root, vi->visual, AllocNone);

    memset(&swa, 0, sizeof swa);
    swa.colormap = cmap;
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask;

    win = SYM(XCreateWindow, g_dpy, root, 0, 0, (unsigned)w, (unsigned)h, 0,
              vi->depth, InputOutput, vi->visual,
              CWColormap | CWBackPixel | CWBorderPixel | CWEventMask, &swa);
    if (!win) {
        fprintf(stderr, "morphdemo: cannot create the window\n");
        return 0;
    }

    /* ask the window manager to leave the size alone */
    memset(&hints, 0, sizeof hints);
    hints.flags = PSize | PMinSize | PMaxSize;
    hints.width = hints.min_width = hints.max_width = w;
    hints.height = hints.min_height = hints.max_height = h;
    SYM(XSetWMNormalHints, g_dpy, win, &hints);
    SYM(XStoreName, g_dpy, win, title);

    /* the close button of the window manager has to reach us */
    {
        Atom del = SYM(XInternAtom, g_dpy, "WM_DELETE_WINDOW", False);
        if (del) SYM(XSetWMProtocols, g_dpy, win, &del, 1);
    }

    g_ctx = SYM(glXCreateContext, g_dpy, vi, NULL, True);
    if (!g_ctx || !SYM(glXMakeCurrent, g_dpy, win, g_ctx)) {
        fprintf(stderr, "morphdemo: cannot create the GL context\n");
        return 0;
    }
    SYM(XFree, vi);

    g_win = win;
    SYM(XMapWindow, g_dpy, g_win);
    SYM(XSync, g_dpy, False);
    g_t0 = plat_now();
    return 1;
}

void plat_done(void) {
    if (g_dpy) {
        SYM(XCloseDisplay, g_dpy);
        g_dpy = NULL;
    }
}

void plat_swap(void) { SYM(glXSwapBuffers, g_dpy, g_win); }

int plat_poll(void) {
    XEvent ev;
    while (SYM(XPending, g_dpy) > 0) {
        SYM(XNextEvent, g_dpy, &ev);
        if (ev.type == KeyPress) {
            /* escape is key code 9 in the standard X keyboard map; the keysym
             * lookup is the second opinion for unusual layouts */
            if (ev.xkey.keycode == 9) return 1;
            if (SYM(XLookupKeysym, &ev.xkey, 0) == XK_Escape) return 1;
        } else if (ev.type == ClientMessage) {
            return 1;                  /* the window manager wants us gone */
        }
    }
    return 0;
}

/* ------------------------------------------------------------- audio ----- */
int plat_audio_open(int rate) {
    if (SYM(snd_pcm_open, &g_pcm, "default", 0 /* playback */, 0) < 0) {
        g_pcm = NULL;
        fprintf(stderr, "morphdemo: no sound device, running silently\n");
        return 0;
    }
    if (SYM(snd_pcm_set_params, g_pcm,
            14 /* SND_PCM_FORMAT_FLOAT_LE */, 3 /* RW_INTERLEAVED */,
            2 /* channels */, (unsigned)rate, 1 /* buffer */, 200000) < 0) {
        SYM(snd_pcm_close, g_pcm);
        g_pcm = NULL;
        fprintf(stderr, "morphdemo: sound device refused the format\n");
        return 0;
    }
    g_rate = rate;
    return 1;
}

int plat_audio_write(const float *buf, int frames) {
    if (g_pcm) {
        long n = SYM(snd_pcm_writei, g_pcm, buf, (unsigned long)frames);
        if (n < 0) {                  /* the device hiccuped: start over */
            SYM(snd_pcm_prepare, g_pcm);
            return 0;
        }
        return (int)n;
    }
    /* No device: keep the production running at the right speed by simply
     * waiting for the block to pass. */
    {
        struct timespec ts;
        ts.tv_sec = frames / g_rate;
        ts.tv_nsec = (long)((frames % g_rate) * 1000000000L / g_rate);
        nanosleep(&ts, NULL);
    }
    return frames;
}

void plat_audio_close(void) {
    if (g_pcm) {
        SYM(snd_pcm_close, g_pcm);
        g_pcm = NULL;
    }
}

/* ------------------------------------------------------------ shaders ---- */
void plat_use(GLuint p) {
    g_prog = p;
    SYM(glUseProgram, p);
}

int plat_uni(const char *name) {
    return SYM(glGetUniformLocation, g_prog, name);
}

GLuint plat_program(const char *vs, const char *fs) {
    GLuint v = SYM(glCreateShader, GL_VERTEX_SHADER);
    GLuint f = SYM(glCreateShader, GL_FRAGMENT_SHADER);
    GLuint p = SYM(glCreateProgram);
    GLint ok = 0;

    SYM(glShaderSource, v, 1, &vs, NULL);
    SYM(glCompileShader, v);
    SYM(glGetShaderiv, v, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei n = 0;
        SYM(glGetShaderInfoLog, v, sizeof log, &n, log);
        fprintf(stderr, "morphdemo: vertex shader failed:\n%.*s\n", (int)n, log);
        return 0;
    }
    SYM(glShaderSource, f, 1, &fs, NULL);
    SYM(glCompileShader, f);
    SYM(glGetShaderiv, f, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei n = 0;
        SYM(glGetShaderInfoLog, f, sizeof log, &n, log);
        fprintf(stderr, "morphdemo: fragment shader failed:\n%.*s\n", (int)n, log);
        return 0;
    }
    p = SYM(glCreateProgram);
    SYM(glAttachShader, p, v);
    SYM(glAttachShader, p, f);
    SYM(glLinkProgram, p);
    SYM(glGetProgramiv, p, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "morphdemo: the shader program does not link\n");
        return 0;
    }
    return p;
}
