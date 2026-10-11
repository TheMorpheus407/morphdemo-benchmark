/*
 * demo4k.c — "MORPH — from noise to order", 4 KiB tier.
 *
 * The same 60-second production as the direct tiers (three phases, MORPH
 * title, synthesized score, Escape quits, clean exit), shrunk to fit inside
 * the self-decompressing morphdemo-4k executable:
 *
 *   - visuals are 277 glow sprites driven through one uniform array (no
 *     per-particle shader code, no fbm, no constellation lines); each
 *     frame is a single glRects() fullscreen draw; slot 276 of the array
 *     carries the time, so one uniform location query serves both;
 *   - every library function is resolved at runtime with dlsym, so the
 *     binary carries only one dynamic symbol (see src/minilib.h, -DMINI);
 *   - the score is a table-driven voice bank (drone, kick, arpeggio, bell)
 *     plus an inharmonic ring-modulated noise bed for phase 1.
 *
 * Sprite slots in u_p[] (x, y, glow k, intensity):
 *   0..191    96 particles, two scales each (halo k=900, core k=9000)
 *   192       central core dot (k=10)
 *   193..275  the 83 lit pixels of the MORPH title (k=9000)
 *   276       the time (read as u_t inside the shader)
 *
 * Timeline (60 s, then clean exit; Escape quits early):
 *   phase 1  EMERGENCE       t =  0..20  scattered drifting particles,
 *                                     drone + noise bed
 *   phase 2  TRANSFORMATION t = 20..40  particles lock onto a 9x5 lattice,
 *                                     beat-pulsed with the kick, arpeggio
 *   phase 3  RESOLUTION     t = 40..60  the ordered lattice slowly rotates,
 *                                     warm core, MORPH title, climax flash
 *                                     at t = 52, fade, exit
 */

#include "minilib.h"

#define W      1920
#define H      1080
#define RATE   48000
#ifndef DUR
#define DUR    60.0
#endif
#define ALAT   0.06
#define NP     96
#define ND     276
#define TAU    6.2831855f

/* ================= freestanding runtime ================= */

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

static double now_s(void)
{
    struct { long sec, nsec; } ts;
    sys6(228, 1, (long)&ts, 0, 0, 0, 0);
    return (double)ts.sec + (double)ts.nsec * 1e-9;
}

static void sleep_us(long us)
{
    struct { long sec, nsec; } ts;
    ts.sec = us / 1000000;
    ts.nsec = (us % 1000000) * 1000;
    sys6(35, (long)&ts, 0, 0, 0, 0, 0);
}

static void dbg(const char *s)
{
    unsigned long n = 0;
    while (s[n]) n++;
    sys6(1, 2, (long)s, (long)n, 0, 0, 0);
}

int run(void);

__attribute__((noreturn)) void _start(void)
{
    __asm__ volatile(
        "xor %rbp, %rbp\n"
        "and $-16, %rsp\n"
        "call run\n"
        "mov %eax, %edi\n"
        "mov $231, %eax\n"
        "syscall\n");
    __builtin_unreachable();
}

/* ================= math (no libm) ================= */

static float floorf_(float x)
{
    float f = (float)(int)x;
    return f > x ? f - 1.0f : f;
}

static float sinf_(float x)
{
    x -= TAU * floorf_(x * 0.15915494f + 0.5f);
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333338f
                     + x2 * (-0.0001984127f + x2 * 2.7557319e-6f))));
}

static float sstep(float a, float b, float x)
{
    x = (x - a) / (b - a);
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

/* ================= music synthesizer =================
 * Table-driven voice bank, driven by the absolute time t so the music is
 * locked to the picture.  Voice = { phase increment, amplitude, envelope
 * decay per sample, retrigger period (0 = once, via huge period), first
 * trigger time }.  The four arpeggio steps are four voices with period
 * 1 s and staggered first triggers, which reproduces the 4-step cycle.
 */

#define NV 7
static const float VT[NV][5] = {
    { 0.00719948f, 0.045f, 1.0f,     1e30f,  0.0f  },   /* drone 55 Hz   */
    { 0.00719948f, 0.85f,  0.99971f, 0.5f,  20.0f  },   /* kick 120 BPM  */
    { 0.17259125f, 0.22f,  0.99998f, 1e30f, 52.0f  },   /* climax bell   */
    { 0.02879793f, 0.11f,  0.99965f, 1.0f,  26.000f },   /* arp step 1    */
    { 0.03424729f, 0.11f,  0.99965f, 1.0f,  26.125f },   /* arp step 2    */
    { 0.03844371f, 0.11f,  0.99965f, 1.0f,  26.250f },   /* arp step 3    */
    { 0.04315000f, 0.11f,  0.99965f, 1.0f,  26.375f },   /* arp step 4    */
};
static float vph[NV], venv[NV], vnxt[NV];
static unsigned char von[NV];

static float gen(float t)
{
    float v = 0.0f;
    for (int i = 0; i < NV; i++) {
        if (t >= vnxt[i]) {
            von[i] = 1;
            venv[i] = 1.0f;
            vph[i] = 0.0f;
            vnxt[i] += VT[i][3];
        }
        if (von[i]) {
            venv[i] *= VT[i][2];
            vph[i] += VT[i][0];
            if (vph[i] > TAU) vph[i] -= TAU;
            v += sinf_(vph[i]) * venv[i] * VT[i][1];
            if (venv[i] < 0.002f) von[i] = 0;
        }
    }
    /* phase 1: inharmonic ring-modulated noise bed (the "noise" the
     * production starts from), fading out by t=23 */
    if (t < 23.0f)
        v += sinf_(t * 2111.0f) * sinf_(t * 6151.0f) * 0.08f
             * (1.0f - sstep(17.0f, 23.0f, t));
    /* master: gain, fade in/out, soft clip */
    v *= 0.8f;
    v *= sstep(0.0f, 1.2f, t) * (1.0f - sstep(58.0f, 59.6f, t));
    v = v / (1.0f + (v < 0.0f ? -v : v));
    return v;
}

/* ================= fragment shader =================
 * All phases, palette, grid, title plate, flash and fade are derived from
 * u_t inside the shader; the CPU only feeds sprite positions/intensities.
 */

static const char FS_SRC[] =
"uniform vec4 u_p[277];\n"
"float s(float a,float b,float x){x=clamp((x-a)/(b-a),0.,1.);"
"return x*x*(3.-2.*x);}\n"
"void main(){float u_t=u_p[276].x;"
"vec2 p=gl_FragCoord.xy/1080.-vec2(.8889,.5);\n"
"float o2=s(20.,38.,u_t),o=s(2.,48.,u_t);\n"
"vec3 b=mix(vec3(.1,.6,.35),vec3(1.,.8,.3),o);\n"
"vec3 c=vec3(0.),t=c;\n"
"for(int i=0;i<276;i++){vec4 d=u_p[i];"
"float g=exp(-dot(p-d.xy,p-d.xy)*d.z)*d.w;"
"if(i<193)c+=b*g;else t+=g;}\n"
"vec2 gf=abs(fract(p*vec2(9.,5.))-.5);\n"
"c+=b*(1.-step(.014,min(gf.x,gf.y)))*o2*.28*(1.-.5*o);\n"
"float pl=(1.-step(.108,abs(p.y+.1765)))"
"*s(42.,45.,u_t)*(1.-s(57.,59.,u_t));\n"
"c=mix(c,c*.12,pl)+t*vec3(1.,.93,.78);\n"
"c+=exp(-abs(u_t-52.)*7.)*.85;\n"
"c=1.-exp(-c*1.6);\n"
"c*=s(0.,1.5,u_t)*(1.-s(58.,59.7,u_t));\n"
"gl_FragColor=vec4(c,1.);}\n";

static GLuint make_prog(void)
{
    const char *src = FS_SRC;
#ifdef DBG
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sh, 1, &src, 0);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, 256, &n, log);
        if (n > 255) n = 255;
        log[n] = 0;
        dbg("morphdemo: shader compile failed:\n");
        dbg(log);
        return 0;
    }
    GLuint pr = glCreateProgram();
    glAttachShader(pr, sh);
    glLinkProgram(pr);
    glGetProgramiv(pr, GL_LINK_STATUS, &ok);
    if (!ok) { dbg("morphdemo: program link failed\n"); return 0; }
    return pr;
#else
    /* one GL 2.0 call compiles and links the fragment shader */
    return glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &src);
#endif
}

/* ================= sprites ================= */

static float hx[NP], hy[NP];
static float up[(ND + 1) * 4];      /* x, y, k, w per sprite; slot ND = time */

/* MORPH in a 5x7 font: 175 bits, bit b = pixel (letter b/35, row
 * (b%35)/5, column b%5), rows top to bottom, columns left to right */
static const unsigned char FT[22] = {
    0x71, 0xd7, 0x18, 0x63, 0x74, 0x31, 0xc6, 0x18, 0xdd, 0x8b, 0xf1,
    0x95, 0x14, 0x5f, 0x8c, 0x2f, 0x84, 0x10, 0x63, 0xfc, 0x31, 0x46 };

static void init_dots(void)
{
    for (int i = 0; i < NP; i++) {
        /* deterministic pseudo-random home in [0,1), via aliased sines */
        hx[i] = sinf_(i * 12.9898f) * .5f + .5f;
        hy[i] = sinf_(i * 78.233f) * .5f + .5f;
        up[8 * i + 2] = 900.0f;
        up[8 * i + 6] = 9000.0f;
    }
    up[4 * 192 + 2] = 10.0f;
    int k = 193;
    for (int b = 0; b < 175; b++)
        if ((FT[b >> 3] >> (b & 7)) & 1) {
            int l = b / 35, r = (b % 35) / 5, c = b % 5;
            up[4 * k] = .127f * (1.5f * l + (c + .5f) / 5.0f - 3.5f);
            up[4 * k + 1] = -.24f + .127f * (1.0f - (r + .5f) / 7.0f);
            up[4 * k + 2] = 50000.0f;   /* tight dots: no bleed into empty cells */
            k++;
        }
}

/* per-frame sprite update: particle i scatters (phase 1) -> lattice
 * (phase 2) -> slowly rotating lattice around the core (phase 3), same
 * formulas as the direct tiers */
static void upd_dots(float t)
{
    float m2 = sstep(20.f, 38.f, t), m3 = sstep(40.f, 48.f, t);
    float text = sstep(42.f, 45.f, t) * (1.f - sstep(57.f, 59.f, t));
    float beat = 0.f;
    if (t >= 20.f) {
        beat = 1.f - 2.f * (t - 0.5f * floorf_(t * 2.0f));
        if (beat < 0.f) beat = 0.f;
    }
    float inten = .35f + .65f * beat;
    /* phase 3: the ordered lattice slowly rotates around the core */
    float ca = sinf_(t * .35f), sa = sinf_(t * .35f + 1.5707964f);
    for (int i = 0; i < NP; i++) {
        float x1 = (hx[i] - .5f) * 1.7f + .05f * sinf_(t * 2.0f + hx[i] * 23.0f);
        float y1 = (hy[i] - .5f) * .95f + .05f * sinf_(t * 1.6f + hy[i] * 23.0f);
        float lx = (floorf_((hx[i] - .5f) * 9.0f) + .5f) / 9.f * 1.7f;
        float ly = (floorf_((hy[i] - .5f) * 5.0f) + .5f) / 5.f * .95f;
        float ax = x1 + (lx - x1) * m2, ay = y1 + (ly - y1) * m2;
        float rx = ax * ca - ay * sa, ry = ax * sa + ay * ca;
        float px = ax + (rx - ax) * m3, py = ay + (ry - ay) * m3;
        float *a = up + 8 * i, *b = a + 4;
        a[0] = px; a[1] = py; a[3] = inten;
        b[0] = px; b[1] = py; b[3] = .9f * inten;
    }
    up[4 * 192 + 3] = (.5f + .5f * beat) * (.3f + .7f * m3);
    float tw = 1.5f * text * (1.3f + .3f * beat);
    for (int i = 193; i < ND; i++)
        up[4 * i + 3] = tw;
}

/* ================= main ================= */

static const int GLA[] = { GLX_RGBA, GLX_DOUBLEBUFFER, 0 };
static const pa_sample_spec SS = { PA_SAMPLE_S16LE, RATE, 2 };

int run(void)
{
    if (!mini_resolve()) return 1;
    Display *d = XOpenDisplay(0);
    if (!d) return 1;
    Window root = XDefaultRootWindow(d);
    XVisualInfo *vi = glXChooseVisual(d, 0, (int *)GLA);
    if (!vi) return 1;

    /* only the fields selected by the value mask are read by XCreateWindow */
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(d, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask;
    swa.override_redirect = 1;
    /* center on the primary monitor: an off-screen override-redirect window
     * is never composited and GLX swaps would stall to ~1 Hz */
    int wx = 0, wy = 0, nm = 0;
    XRRMonitorInfo *mi = XRRGetMonitors(d, root, 0, &nm);
    if (mi && nm > 0) {
        int b = 0;
        for (int i = 0; i < nm; i++)
            if (mi[i].primary) { b = i; break; }
        wx = mi[b].x + (mi[b].width - W) / 2;
        wy = mi[b].y + (mi[b].height - H) / 2;
    }
    Window win = XCreateWindow(d, root, wx, wy, W, H, 0, vi->depth, InputOutput,
                               vi->visual,
                               CWColormap | CWEventMask | CWOverrideRedirect,
                               &swa);
    XStoreName(d, win, "MORPH");
    XMapWindow(d, win);
    XSetInputFocus(d, win, RevertToPointerRoot, CurrentTime);

    GLXContext ctx = glXCreateContext(d, vi, 0, 1);
    if (!ctx) return 1;
    glXMakeCurrent(d, win, ctx);

    GLuint prog = make_prog();
    if (!prog) return 1;
    glUseProgram(prog);
    /* slot ND of u_p carries the time, so one location query is enough */
    GLint u_p = glGetUniformLocation(prog, "u_p");
    if (u_p < 0) return 1;
    init_dots();
    for (int i = 0; i < NV; i++)
        vnxt[i] = VT[i][4];

    int aerr = 0;
    pa_simple *pa = pa_simple_new(0, "morphdemo", PA_STREAM_PLAYBACK, 0,
                                  "morphdemo", &SS, 0, 0, &aerr);

    static short sbuf[4096 * 2];
    double t0 = now_s();
    double taudio = 0.0;
    int quit = 0;

    while (!quit) {
        double t = now_s() - t0;
        if (t >= DUR) break;

        XEvent ev;
        while (XCheckWindowEvent(d, win, KeyPressMask, &ev))
            if (ev.xkey.keycode == 9) { quit = 1; break; }   /* Escape */
        if (quit) break;

        upd_dots((float)t);
        up[4 * ND] = (float)t;
        glUniform4fv(u_p, ND + 1, up);
        glRects(-1, -1, 1, 1);
        glXSwapBuffers(d, win);

        /* push the audio that has come due (real-time rate) */
        if (pa) {
            double due = t + ALAT - taudio;
            long n = (long)(due * RATE);
            if (n > 4096) n = 4096;
            if (n >= 256) {
                float at = (float)taudio;
                for (long i = 0; i < n; i++) {
                    short v = (short)(gen(at) * 32767.0f);
                    sbuf[2 * i] = v;
                    sbuf[2 * i + 1] = v;
                    at += 1.0f / RATE;
                }
                if (pa_simple_write(pa, sbuf, (unsigned long)n * 4, &aerr) < 0)
                    break;
                taudio += (double)n / RATE;
            }
        }

        /* cap the frame rate if vsync is off */
        double el = now_s() - t0 - t;
        if (el < 0.012)
            sleep_us((long)((0.012 - el) * 1e6));
    }

    dbg("morphdemo: done\n");
    return 0;
}
