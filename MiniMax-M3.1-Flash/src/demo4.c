/* demo4.c -- "SEED", the four kilobyte production.
 *
 * A field of sparks drifts through a turbulent void, is pulled onto a regular
 * lattice, and finally collapses into the word MORPH while the music settles
 * into a chord.
 *
 * The file is deliberately self contained: at four kilobytes every byte
 * counts, so the window, the sound device and the GL entry points are handled
 * here, and every symbol is resolved at runtime with dlsym instead of through
 * a PLT entry.  The picture comes from one fragment shader, which is by far
 * the cheapest way to get a rich image out of a tiny binary.
 *
 * The wall clock is the clock.  The sound device is fed exactly as much as
 * the clock asks for, and the number of samples already written to it defines
 * the time of the frame that is being drawn, so the image cannot drift away
 * from the music.
 */
#include <dlfcn.h>
#include <time.h>

#include "x11min.h"
#include "glconst.h"

/* ------------------------------------------------------- entry points --- */
#define SYMS(X)                                                             \
    X(XOpenDisplay, Display *, (const char *))                             \
    X(XDefaultRootWindow, Window, (Display *))                             \
    X(XCreateWindow, Window, (Display *, Window, int, int, unsigned int,    \
                              unsigned int, unsigned int, int, unsigned int, \
                              Visual *, unsigned long, XSetWindowAttributes *)) \
    X(XCreateColormap, Colormap, (Display *, Window, Visual *, int))       \
    X(XMapWindow, int, (Display *, Window))                                 \
    X(XPending, int, (Display *))                                          \
    X(XNextEvent, int, (Display *, XEvent *))                              \
    X(glXChooseVisual, XVisualInfo *, (Display *, int, int *))             \
    X(glXCreateContext, GLXContext, (Display *, XVisualInfo *, GLXContext, Bool)) \
    X(glXMakeCurrent, Bool, (Display *, GLXDrawable, GLXContext))          \
    X(glXSwapBuffers, void, (Display *, GLXDrawable))                      \
    X(glCreateShader, GLuint, (GLenum))                                    \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(glCompileShader, void, (GLuint))                                     \
    X(glCreateProgram, GLuint, (void))                                     \
    X(glAttachShader, void, (GLuint, GLuint))                              \
    X(glLinkProgram, void, (GLuint))                                       \
    X(glUseProgram, void, (GLuint))                                        \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar *))                \
    X(glUniform1f, void, (GLint, GLfloat))                                 \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei))                         \
    X(snd_pcm_open, int, (void **, const char *, int, int))                     \
    X(snd_pcm_set_params, int, (void *, int, int, unsigned int, unsigned int,   \
                                int, unsigned int))                             \
    X(snd_pcm_writei, long, (void *, const void *, unsigned long))             \
    X(snd_pcm_prepare, int, (void *))                                         \
    X(write, long, (int, const void *, unsigned long))                       \
    X(clock_gettime, int, (int, struct timespec *))

enum {
#define X(n, r, p) I_##n,
    SYMS(X)
#undef X
    NSYMS
};

#define X(n, r, p) typedef r (*PFN_##n) p;
SYMS(X)
#undef X

static void *g_sym[NSYMS];
static const char g_names[] =
#define X(n, r, p) #n "\0"
    SYMS(X)
#undef X
    ;

#define SYM(n, ...) ((PFN_##n)g_sym[I_##n])(__VA_ARGS__)

static void sym_load(void) {
    const char *p = g_names;
    int i;
    for (i = 0; i < NSYMS; i++) {
        g_sym[i] = dlsym(RTLD_DEFAULT, p);
        while (*p++) ;
    }
}

/* ----------------------------------------------------------- shaders ----- */
/* One triangle covers the screen; the fragment shader does the rest. */
static const char VS[] =
    "#version 330\n"
    "void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);\n"
    "gl_Position=vec4(p*4.-1.,0.,1.);}\n";

/* The five letters, each packed into 30 bits: six rows of five columns,
 * top row first, leftmost column in the highest bit. */
static const char FS[] =
    "#version 330\n"
    "uniform float T;\n"
    "const uint F[5]=uint[5](0x23bac631u,0x1d18c62eu,0x3d18fa92u,0x3d18fa10u,0x231fc631u);\n"
    "float n(vec2 p){return .5+.5*sin(p.x+1.7*sin(p.y*1.3)+.9*sin(p.x*.7+p.y*.5));}\n"
    "void main(){\n"
    "vec2 uv=(gl_FragCoord.xy-vec2(960.,540.))/540.,g=uv/.115;\n"
    "float t=T,o=smoothstep(10.8,37.2,t),bf=fract(t*1.5),bq=t/60.;\n"
    "vec2 c=floor(g),p=c*.6+t*.3;\n"
    "vec2 q=fract(g)-.5-(vec2(n(p),n(p+7.3))-.5)*(1.-o)*.95;\n"
    "float d=length(q),co=exp(-d*d*160.),gl=exp(-d*d*18.);\n"
    "int ci=int(floor(g.x))+14,ri=int(floor(g.y))+3,li=ci/6,lc=ci-li*6;float bt=0.,ig=0.;\n"
    "if(ci>=0&&ci<29&&ri>=0&&ri<6){if(lc<5)bt=float((F[li]>>uint(29-ri*5-lc))&1u);\n"
    "ig=step(.66+float(li)*.025,bq);}\n"
    "float L=mix(1.,bt*ig,smoothstep(37.2,46.8,t));\n"
    "vec3 col=vec3(.02,.03,.07)+n(uv*2.+t*.05)*.04*(1.-.5*o);\n"
    "col+=L*(co*1.6+gl*.3)*mix(vec3(.4,.6,1.),vec3(1.,.75,.3),o)*(.7+.5*pow(1.-bf,3.));\n"
    "col*=1.-smoothstep(56.5,60.,t);\n"
    "gl_FragColor=vec4(col,1.);}\n";

/* ------------------------------------------------------------ audio ------ */
static const float SC[5] = { 1.f, 1.19f, 1.33f, 1.5f, 1.78f };
static float ap[4];
static unsigned int rng;

/* Printed when the machine has no sound device at all. */
static const char NOSOUND[] = "morphdemo: no sound device, running silently\n";

/* A cheap sine, good enough for a drone and for the pulse voices. */
static float fsin(float x) {
    float k = x * 0.15915494f;
    k -= (float)(int)k;
    k -= k > 0.5f;
    x = k * 6.2831853f;
    float x2 = x * x;
    return x * (1.f + x2 * (-0.1666667f + x2 * 0.0083333f));
}

/* One block of stereo audio.  A drone is always present, a noise bed marks
 * the chaotic opening, a pulse enters when the lattice forms and a pentatonic
 * line resolves into the final chord. */
static void synth(float *out, int n, float t0) {
    for (int i = 0; i < n; i++) {
        float t = t0 + i * (1.f / 48000.f);
        float bt = t * 1.5f;
        float bf = bt - (float)(int)bt;
        float s = 0.f;

        ap[0] += 0.00119f;                    /* drone */
        s += fsin(ap[0]) * 0.20f;
        ap[1] += 0.00121f;
        s += fsin(ap[1]) * 0.13f;

        if (t < 20.f) {                       /* the noise of the opening */
            rng = rng * 22695477u + 1u;
            s += ((float)(rng >> 9) * 1.2e-7f - 1.f) * 0.08f * (1.f - t / 20.f);
        }

        if (t > 14.f) {                       /* the beat that orders it */
            float e = 1.f - bf * 3.f;
            if (e > 0.f) {
                ap[2] += 0.042f + 0.10f * e;
                s += fsin(ap[2]) * e * e * 0.50f;
            } else {
                ap[2] += 0.142f;
            }
        }

        if (t > 24.f) {                       /* the line that resolves */
            int st = (int)(bt * 2.f);
            int k = (int)((0x49u >> (3 * (st & 7))) & 7u);
            ap[3] += 220.f * SC[k % 5] * (t > 42.f ? 2.f : 1.f) * (1.f / 48000.f);
            ap[3] -= ap[3] >= 1.f;
            float sq = ap[3] < 0.5f ? 1.f : -1.f;
            s += sq * (1.f - bf) * (1.f - bf) * 0.06f * (t > 34.f ? 1.f : (t - 24.f) * 0.1f);
        }

        s *= (t < 2.5f ? t * t * 0.16f : 1.f) * (t > 56.f ? (60.f - t) * 0.25f : 1.f);
        out[2 * i] = out[2 * i + 1] = s * 0.8f;
    }
}

/* ------------------------------------------------------------ main ------- */
int main(void) {
    /* A double buffered RGBA visual and a legacy context.  This driver hands
     * out a 4.6 compatibility profile for the latter, so shaders work. */
    static const int vattrs[] = { 4, 8, 8, 9, 8, 10, 8, 5, 1, 0 };
    XSetWindowAttributes swa;
    XVisualInfo *vi;
    const char *sv = VS, *sf = FS;
    Display *dpy;
    Window root, win;
    Colormap cmap;
    GLXContext ctx;
    void *pcm = 0;
    GLuint vs, fs, prg;
    GLint uloc;
    float blk[4800];
    long sent = 0, s0 = 0, need;
    struct timespec ts;

    sym_load();
    dpy = SYM(XOpenDisplay, NULL);
    if (!dpy) return 1;
    root = SYM(XDefaultRootWindow, dpy);
    vi = SYM(glXChooseVisual, dpy, 0, (int *)vattrs);
    if (!vi) return 1;
    cmap = SYM(XCreateColormap, dpy, root, vi->visual, 0);
    swa.colormap = cmap;
    swa.event_mask = KeyPressMask;
    win = SYM(XCreateWindow, dpy, root, 0, 0, 1920, 1080, 0, vi->depth, InputOutput,
              vi->visual, CWColormap | CWEventMask, &swa);
    ctx = SYM(glXCreateContext, dpy, vi, NULL, True);
    if (!ctx || !SYM(glXMakeCurrent, dpy, win, ctx)) return 1;
    SYM(XMapWindow, dpy, win);

    vs = SYM(glCreateShader, GL_VERTEX_SHADER);
    SYM(glShaderSource, vs, 1, &sv, NULL);
    SYM(glCompileShader, vs);
    fs = SYM(glCreateShader, GL_FRAGMENT_SHADER);
    SYM(glShaderSource, fs, 1, &sf, NULL);
    SYM(glCompileShader, fs);
    prg = SYM(glCreateProgram);
    SYM(glAttachShader, prg, vs);
    SYM(glAttachShader, prg, fs);
    SYM(glLinkProgram, prg);
    SYM(glUseProgram, prg);
    uloc = SYM(glGetUniformLocation, prg, "T");

    /* The wall clock is the clock of the production.  The sound device is fed
     * exactly as much as that clock asks for, so the music always runs at its
     * own speed and the picture follows it. */
    if (SYM(snd_pcm_open, &pcm, "default", 0, 0) == 0 &&
        SYM(snd_pcm_set_params, pcm, 14, 3, 2, 48000, 1, 200000) == 0) {
        /* float little endian, interleaved, stereo, 48 kHz */
    } else {
        /* No device means no music.  Say so, do not pretend. */
        pcm = 0;
        SYM(write, 2, NOSOUND, sizeof NOSOUND - 1);
    }

    SYM(clock_gettime, CLOCK_MONOTONIC, &ts);
    s0 = ts.tv_sec * 48000L + (ts.tv_nsec * 48L) / 1000000L;

    for (;;) {
        XEvent ev;
        if (SYM(XPending, dpy) > 0) {
            SYM(XNextEvent, dpy, &ev);
            if (ev.type == KeyPress && ev.xkey.keycode == 9) return 0;
        }
        /* The wall clock is the clock of the production and the sound device
         * is fed exactly as much as that clock asks for: the music therefore
         * always runs at its own speed and the picture follows it. */
        SYM(clock_gettime, CLOCK_MONOTONIC, &ts);
        need = (long)(ts.tv_sec * 48000L + (ts.tv_nsec * 48L) / 1000000L) - s0 - sent;
        if (need < 0) need = 0;
        if (need > 2400) need = 2400;
        if (need > 0) {
            long n = need;
            if (pcm) {
                synth(blk, (int)need, (float)sent * (1.f / 48000.f));
                n = SYM(snd_pcm_writei, pcm, blk, (unsigned long)need);
                if (n < 0) { SYM(snd_pcm_prepare, pcm); n = 0; }
            }
            sent += n;
        }
        if (sent > 2880000L) break;
        SYM(glUniform1f, uloc, (float)sent / 48000.f);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
        SYM(glXSwapBuffers, dpy, win);
    }
    return 0;
}
