/*
 * morphdemo.c — "MORPH — from noise to order"
 *
 * A real-time audiovisual demo (OpenGL fragment shader + synthesized audio)
 * built as a freestanding native Linux executable with no libc dependency:
 * the binary provides its own _start, syscall wrappers and math, and links
 * only against the system runtime libraries libX11, libGL, libXrandr and
 * libpulse-simple.
 *
 * The whole production is a pure function of one clock: the fragment shader
 * derives every visual phase from the uniform u_t, and the music is
 * synthesized sample-by-sample from the same absolute time, so sound and
 * picture are locked together by construction.  The window is placed on
 * the primary monitor (an off-screen window is never composited and GLX
 * swaps would stall to ~1 Hz).
 *
 * Timeline (60 s, then clean exit; Escape quits early):
 *   phase 1  EMERGENCE      t =  0..20   noise clouds, scattered twinkling
 *                                    particles, sparse pings, low drone
 *   phase 2  TRANSFORMATION t = 20..40   particles lock onto a lattice and
 *                                    stream, constellation lines, kick,
 *                                    hi-hats, arpeggio, noise bed fades
 *   phase 3  RESOLUTION     t = 40..60   particles collapse into a rotating
 *                                    ring, mandala, MORPH title, climax
 *                                    flash at t = 52, fade out, exit
 *
 * Three build tiers share this engine (selected with -DTIER=n):
 *   TIER 2 -> morphdemo-64k   richest: 96 particles, constellation lines,
 *                             5-octave fbm, echo, hats, full score
 *   TIER 1 -> morphdemo-16k   64 particles, 5-octave fbm, echo, hats
 *   TIER 0 -> (unused)        40 particles, 3-octave fbm, essential score
 * The 4 KiB tier (morphdemo-4k) is a separate compact program, src/demo4k.c,
 * shipped inside the self-decompressing stub.
 */

#include "minilib.h"

#ifndef TIER
#define TIER 2
#endif

#define W      1920
#define H      1080
#define RATE   48000
#ifndef DUR
#define DUR    60.0            /* total production length in seconds */
#endif
#define ALAT   0.06            /* audio latency compensation (s) */
#define TWOPI  6.283185307179586

/* ================= freestanding runtime ================= */

void *memset(void *d, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}

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

static double now_s(void)                       /* CLOCK_MONOTONIC = 1 */
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

static void dbg_log(const char *s, long n)
{
    sys6(1, 2, (long)s, n, 0, 0, 0);
}

int run(void);

__attribute__((noreturn)) void _start(void)
{
    __asm__ volatile(
        "xor %rbp, %rbp\n"
        "and $-16, %rsp\n"
        "call run\n"
        "mov %eax, %edi\n"
        "mov $231, %eax\n"      /* exit_group: kill helper threads too */
        "syscall\n");
    __builtin_unreachable();
}

/* ================= math (no libm) ================= */

static double my_floor(double x)
{
    double f = (double)(long long)x;
    return f > x ? f - 1.0 : f;
}

static double my_sin(double x)
{
    x -= TWOPI * my_floor(x / TWOPI + 0.5);      /* |x| <= pi */
    double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6.0 + x2 * (1.0 / 120.0 + x2 * (-1.0 / 5040.0
                     + x2 * (1.0 / 362880.0)))));
}

static float fabsf_(float x) { return x < 0.0f ? -x : x; }

static float clamp01(float x)
{
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

static float sstep(float a, float b, float x)
{
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

/* ================= music synthesizer =================
 * One function, called once per output sample, in time order.
 * All voices are driven by the absolute time t so the score is
 * deterministic and locked to the visuals (same clock, same formulas).
 */

static unsigned lcg = 0x2545F491u;

static float rnd01(void)
{
    lcg = lcg * 1664525u + 1013904223u;
    return (float)((lcg >> 8) & 0xFFFFFF) * (1.0f / 16777216.0f);
}

#if TIER >= 1
#define ECHO_N 16384
static float echo_buf[ECHO_N];
static int echo_pos;
#endif

static double ph_d1, ph_d2;          /* drone */
static double ph_ping, ph_kick, ph_arp, ph_bell;
static double ph_pad[4];
static double next_ping = 0.6, next_kick = 20.0;
static double next_arp = 26.0;
#if TIER >= 1
static double next_hat = 24.25;
#endif
static float kick_env, kick_pit, arp_env, arp_f = 440.0f;
static float ping_env, ping_f, bell_env;
#if TIER >= 1
static float hat_env;
#endif
static float nlp;                    /* noise bed lowpass state */
#if TIER >= 1
static float nhp;                    /* hi-hat highpass state */
#endif
static int kick_on, arp_on, ping_on, bell_on;
#if TIER >= 1
static int hat_on;
#endif
static int arp_i;

static const float ARP_TAB[8] = { 220.00f, 261.63f, 293.66f, 329.63f,
                                  392.00f, 440.00f, 523.25f, 587.33f };
static const float PAD_TAB[4] = { 110.00f, 164.81f, 220.00f, 261.63f };

static float gen(float t)
{
    float v = 0.0f;

    /* --- drone: two sines an octave below the pad, slow tremolo --- */
    ph_d1 += TWOPI * 55.0 / RATE;  if (ph_d1 > TWOPI) ph_d1 -= TWOPI;
    ph_d2 += TWOPI * 82.41 / RATE; if (ph_d2 > TWOPI) ph_d2 -= TWOPI;
    float drone = 0.045f * (float)my_sin(ph_d1) + 0.028f * (float)my_sin(ph_d2);
    v += drone * (0.75f + 0.25f * (float)my_sin(t * 0.22));

    /* --- phase 1: filtered noise bed, fading out by t=23 --- */
    if (t < 23.0f) {
        float n = rnd01() * 2.0f - 1.0f;
        nlp += (n - nlp) * 0.015f;
        v += nlp * 0.11f * (1.0f - sstep(17.0f, 23.0f, t));
    }

    /* --- phase 1: sparse random pings --- */
    if (t < 20.0f && t >= next_ping) {
        float r = rnd01();
        next_ping += 0.45f + r * 0.8f;
        if (r < 0.55f) {
            ping_on = 1;
            ping_f = 330.0f + rnd01() * 660.0f;
            ping_env = 1.0f;
            ph_ping = 0.0;
        }
    }
    if (ping_on) {
        ping_env *= 0.99955f;
        ph_ping += TWOPI * ping_f / RATE; if (ph_ping > TWOPI) ph_ping -= TWOPI;
        v += (float)my_sin(ph_ping) * ping_env * 0.16f;
        if (ping_env < 0.002f) ping_on = 0;
    }

    /* --- phase 2+: kick on every half second (120 BPM), t=20.. --- */
    if (t >= next_kick) {
        kick_on = 1;
        kick_env = 1.0f;
        kick_pit = 1.0f;
        ph_kick = 0.0;
        next_kick += 0.5f;
    }
    if (kick_on) {
        kick_env *= 0.9997083f;              /* exp(-14 * tau) */
        kick_pit *= 0.9994167f;              /* pitch drop 156 -> 46 Hz */
        ph_kick += TWOPI * (46.0 + 110.0 * kick_pit) / RATE;
        if (ph_kick > TWOPI) ph_kick -= TWOPI;
        v += (float)my_sin(ph_kick) * kick_env * 0.85f;
        if (kick_env < 0.003f) kick_on = 0;
    }

#if TIER >= 1
    /* --- phase 2+: off-beat hi-hats --- */
    if (t >= next_hat) { hat_on = 1; hat_env = 1.0f; next_hat += 0.25f; }
    if (hat_on) {
        float n = rnd01() * 2.0f - 1.0f;
        float hp = n - nhp;
        nhp += (n - nhp) * 0.6f;
        v += hp * hat_env * 0.10f;
        hat_env *= 0.995f;
        if (hat_env < 0.02f) hat_on = 0;
    }
#endif

    /* --- phase 2+: 16th-note arpeggio (A minor) --- */
    if (t >= next_arp) {
        arp_i = (arp_i + 1) & 7;
        arp_f = ARP_TAB[arp_i];
        arp_on = 1;
        arp_env = 1.0f;
        ph_arp = 0.0;
        next_arp += 0.125f;
    }
    if (arp_on) {
        arp_env *= 0.99965f;
        ph_arp += TWOPI * arp_f / RATE; if (ph_arp > TWOPI) ph_arp -= TWOPI;
        float s = (float)my_sin(ph_arp) + 0.30f * (float)my_sin(2.0 * ph_arp);
        float a = s * arp_env * 0.11f;
        v += a;
#if TIER >= 1
        float d = echo_buf[echo_pos];
        echo_buf[echo_pos] = a + d * 0.38f;
        echo_pos = (echo_pos + 1) & (ECHO_N - 1);
        v += d * 0.55f;
#endif
        if (arp_env < 0.002f) arp_on = 0;
    }

    /* --- phase 3: warm pad chord swell from t=40 --- */
    if (t >= 40.0f) {
        float swell = sstep(40.0f, 47.0f, t) * (1.0f - sstep(57.0f, 59.4f, t));
        for (int i = 0; i < 4; i++) {
            ph_pad[i] += TWOPI * PAD_TAB[i] / RATE;
            if (ph_pad[i] > TWOPI) ph_pad[i] -= TWOPI;
            v += (float)my_sin(ph_pad[i] + 0.004f * i) * 0.040f * swell;
        }
    }

    /* --- climax bell at t=52 (synced with the visual flash) --- */
    if (t >= 52.0f && !bell_on) { bell_on = 1; bell_env = 1.0f; ph_bell = 0.0; }
    if (bell_on) {
        bell_env *= 0.9999826f;              /* ~1.2 s tail */
        ph_bell += TWOPI * 1318.5 / RATE; if (ph_bell > TWOPI) ph_bell -= TWOPI;
        v += ((float)my_sin(ph_bell) + 0.45f * (float)my_sin(ph_bell * 2.003)
              + 0.20f * (float)my_sin(ph_bell * 3.01)) * bell_env * 0.22f;
        if (bell_env < 0.002f) bell_on = 0;
    }

    /* --- master: gain, fade in/out, soft clip --- */
    v *= 0.8f;
    v *= sstep(0.0f, 1.2f, t) * (1.0f - sstep(58.0f, 59.6f, t));
    v = v / (1.0f + fabsf_(v));
    return v;
}

/* ================= fragment shader =================
 * Everything (phases, beat, title, fade) is derived from u_t inside the
 * shader, so the picture can never drift from the score.
 */

static const char FS_SRC[] =
"#version 110\n"
"uniform float u_t;\n"
#if TIER == 2
"#define PN 96\n#define AN .0654\n"
#elif TIER == 1
"#define PN 64\n#define AN .0982\n"
#else
"#define PN 40\n#define AN .1571\n"
#endif
"float hash21(vec2 p){p=fract(p*vec2(123.34,456.21));p+=dot(p,p+45.32);return fract(p.x*p.y);}\n"
"float vnoise(vec2 p){vec2 i=floor(p),f=fract(p);vec2 u=f*f*(3.-2.*f);"
"return mix(mix(hash21(i),hash21(i+vec2(1.,0.)),u.x),"
"mix(hash21(i+vec2(0.,1.)),hash21(i+vec2(1.,1.)),u.x),u.y);}\n"
#if TIER >= 1
"float fbm(vec2 p){float v=0.,a=.5;for(int i=0;i<5;i++){v+=a*vnoise(p);p=p*2.03+vec2(4.7,9.2);a*=.5;}return v;}\n"
#else
"float fbm(vec2 p){float v=0.,a=.5;for(int i=0;i<3;i++){v+=a*vnoise(p);p=p*2.03+vec2(4.7,9.2);a*=.5;}return v;}\n"
#endif
"float sstep(float a,float b,float x){x=clamp((x-a)/(b-a),0.,1.);return x*x*(3.-2.*x);}\n"
/* particle i: scattered (phase 1) -> lattice + flow (phase 2) -> ring (phase 3) */
"vec2 ppos(int i,float t){\n"
" vec2 h=vec2(hash21(vec2(float(i),1.7)),hash21(vec2(float(i),7.3)));\n"
" vec2 t1=(h-.5)*vec2(1.7,.95)+.05*vec2(sin(t*2.+h.x*23.),cos(t*1.6+h.y*23.));\n"
" vec2 gp=(floor((h-.5)*vec2(9.,5.))+.5)/vec2(9.,5.)*vec2(1.7,.95);\n"
" vec2 t2=mix(t1,gp,.85)+.06*vec2(sin(t*3.+h.y*31.),cos(t*2.4+h.x*31.));\n"
" float ang=float(i)*AN+h.x*.25+t*.35;\n"
" vec2 t3=vec2(cos(ang),sin(ang))*(.06+.40*h.y);\n"
" return mix(mix(t1,t2,sstep(20.,38.,t)),t3,sstep(40.,48.,t));}\n"
/* 5x7 bitmap font, rows packed 5 bits per row into two floats per letter */
"float rowv(float f1,float f2,float r){float v=f1,k=1.;if(r>=4.){v=f2;r-=4.;}"
"if(r>=1.){k=.03125;if(r>=2.){k=.0009765625;if(r>=3.)k=.000030517578125;}}return floor(v*k);}\n"
"float bitv(float v,float c){float k=1.;if(c>=1.){k=.5;if(c>=2.){k=.25;if(c>=3.){k=.125;if(c>=4.)k=.0625;}}}"
"return mod(floor(v*k),2.);}\n"
"float chr(vec2 q,float f1,float f2,float w){"
"if(q.x<0.||q.x>1.||q.y<0.||q.y>1.)return 0.;"
"float gy=(1.-q.y)*7.;"
"float b=bitv(rowv(f1,f2,floor(gy)),floor(q.x*5.));"
"vec2 f=fract(vec2(q.x*5.,gy))-.5;"
"return b*(1.-smoothstep(w*.3,w*.52,length(f)));}\n"
"float word(vec2 q,float w){float m=0.;\n"
" m=max(m,chr(vec2(q.x,q.y),579441.,17969.,w));\n"      /* M */
" m=max(m,chr(vec2(q.x-1.5,q.y),575022.,14897.,w));\n"   /* O */
" m=max(m,chr(vec2(q.x-3.,q.y),509487.,17701.,w));\n"    /* R */
" m=max(m,chr(vec2(q.x-4.5,q.y),509487.,1057.,w));\n"    /* P */
" m=max(m,chr(vec2(q.x-6.,q.y),1033777.,17969.,w));\n"   /* H */
" return m;}\n"
/* per-tier palette: cold noise -> ordered climax color */
"vec3 pcol(float x){\n"
#if TIER == 0
" return mix(vec3(.1,.6,.35),vec3(1.,.8,.3),x);\n"
#elif TIER == 1
" return mix(vec3(.2,.4,.9),vec3(.9,.4,.8),x);\n"
#else
" return mix(vec3(.15,.3,.65),vec3(1.,.85,.5),x);\n"
#endif
"}\n"
"void main(){\n"
" vec2 p=gl_FragCoord.xy/vec2(1920.,1080.)-.5;\n"
" p.x*=1920./1080.;\n"
" float o1=sstep(2.,20.,u_t),o2=sstep(20.,38.,u_t),o3=sstep(40.,48.,u_t);\n"
" float text=sstep(42.,45.,u_t)*(1.-sstep(57.,59.,u_t));\n"
" float beat=0.;\n"
" if(u_t>=20.)beat=exp(-14.*(u_t-.5*floor(u_t/.5)));\n"
" if(u_t>=52.)beat+=exp(-3.4*(u_t-52.))*1.5;\n"
" float cp=.4*o1+.3*o2+.3*o3;\n"
" vec3 base=pcol(cp);\n"
" vec3 col=vec3(0.);\n"
/* emergence: fbm noise clouds */
" float n=fbm(p*2.2+vec2(u_t*.12,u_t*.08));\n"
" col+=mix(vec3(.015,.03,.07),base*.9,n*n)*mix(.9,.08,o1);\n"
/* particles */
" vec3 pc=vec3(0.);\n"
" for(int i=0;i<PN;i++){\n"
"  vec2 pp=ppos(i,u_t);\n"
"  vec2 dd=p-pp;\n"
"  float d2=dot(dd,dd);\n"
"  float g=exp(-d2*900.)*(.55+.45*sin(u_t*3.+float(i)*1.7))+exp(-d2*9000.)*.9;\n"
"  pc+=g*mix(base,vec3(1.),.55+.45*hash21(vec2(float(i),3.3)));\n"
" }\n"
" col+=pc*(.35+.65*beat);\n"
#if TIER == 2
/* transformation: constellation lines between neighbouring particles */
" float lines=0.;\n"
" for(int i=0;i<PN;i++){\n"
"  int j=i+1;if(j>=PN)j=0;\n"
"  vec2 a=ppos(i,u_t),b=ppos(j,u_t);\n"
"  vec2 ab=b-a;\n"
"  float hh=clamp(dot(p-a,ab)/dot(ab,ab),0.,1.);\n"
"  float d=length(p-(a+ab*hh));\n"
"  lines+=exp(-d*d*900.);\n"
" }\n"
" col+=base*lines*.22*(.3+.7*o2);\n"
#endif
/* transformation: lattice grid */
" vec2 gf=abs(fract(p*vec2(9.,5.))-.5);\n"
" col+=base*(1.-smoothstep(0.,.014,min(gf.x,gf.y)))*o2*.28*(1.-.5*o3);\n"
/* resolution: mandala + core */
" float r=length(p),a=atan(p.y,p.x);\n"
" float mand=(.5+.5*cos(r*26.-u_t*1.2))*(.5+.5*cos(a*14.+r*16.-u_t*.6))*o3*.30;\n"
" col+=mix(base,vec3(1.),.7)*mand*(.6+.4*beat);\n"
" col+=vec3(1.,.95,.85)*exp(-r*r*10.)*(.5+.5*beat)*(.3+.7*o3);\n"
/* the title: word centered on screen, on a dark plate so it stays
   legible over the bright core glow behind it */
" vec2 q=(p-vec2(0.,-.24))/(.127*(.75+.25*text))+vec2(3.5,0.);\n"
" float plate=(1.-smoothstep(3.9,4.4,abs(q.x-3.5)))*(1.-smoothstep(.62,.85,abs(q.y-.5)))*text;\n"
" col=mix(col,col*.12,plate);\n"
" col+=(word(q,1.)+.4*word(q,2.6))*vec3(1.,.93,.78)*text*(1.3+.3*beat);\n"
/* climax flash + shockwave at t=52 */
" float fl=exp(-abs(u_t-52.)*7.);\n"
" col+=fl*.85+exp(-abs(r-(u_t-52.)*.22)*45.)*fl*.5*vec3(.9,.95,1.);\n"
/* vignette, tonemap, master fade */
" col*=1.-.42*dot(p,p);\n"
" col=1.-exp(-col*1.6);\n"
" col*=sstep(0.,1.5,u_t)*(1.-sstep(58.,59.7,u_t));\n"
" gl_FragColor=vec4(col,1.);\n"
"}\n";

static GLuint make_prog(void)
{
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    const char *src = FS_SRC;
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
        dbg_log(log, n);
        return 0;
    }
    GLuint pr = glCreateProgram();
    glAttachShader(pr, sh);
    glLinkProgram(pr);
    glGetProgramiv(pr, GL_LINK_STATUS, &ok);
    if (!ok) {
        dbg("morphdemo: program link failed\n");
        return 0;
    }
    return pr;
}

/* ================= main =================
 * One loop, one clock: each iteration renders one frame and pushes the
 * audio that has come due to the sound server.  The loop is paced by the
 * display refresh (GLX swap), so the score is written at exactly real-time
 * rate; the ALAT lookahead keeps the heard sound aligned with the picture.
 */

int run(void)
{
#ifdef MINI
    if (!mini_resolve()) { dbg("morphdemo: symbol resolve failed\n"); return 1; }
#endif
    Display *d = XOpenDisplay(0);
    if (!d) { dbg("morphdemo: cannot open display\n"); return 1; }
    Window root = XDefaultRootWindow(d);
    int attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                   GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, 0 };
    XVisualInfo *vi = glXChooseVisual(d, 0, attr);
    if (!vi) { dbg("morphdemo: no GLX visual\n"); return 1; }

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof swa);
    swa.colormap = XCreateColormap(d, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask;
    swa.override_redirect = 1;
    /* Center the window on the primary monitor.  It must sit on a visible
     * output: an off-screen override-redirect window is never composited,
     * no frame callbacks arrive, and GLX buffer release stalls to ~1 Hz. */
    int wx = 0, wy = 0;
    int nm = 0;
    XRRMonitorInfo *mi = XRRGetMonitors(d, root, 1, &nm);
    if (!mi || nm <= 0)
        mi = XRRGetMonitors(d, root, 0, &nm);
    if (mi && nm > 0) {
        int best = 0;
        long bestarea = -1;
        for (int i = 0; i < nm; i++) {
            if (mi[i].primary) { best = i; break; }
            long a = (long)mi[i].width * mi[i].height;
            if (a > bestarea) { bestarea = a; best = i; }
        }
        wx = mi[best].x + (mi[best].width - W) / 2;
        wy = mi[best].y + (mi[best].height - H) / 2;
        XRRFreeMonitors(mi);
    }
    Window win = XCreateWindow(d, root, wx, wy, W, H, 0, vi->depth, InputOutput,
                               vi->visual,
                               CWColormap | CWEventMask | CWOverrideRedirect,
                               &swa);
    XStoreName(d, win, "MORPH");
    XMapWindow(d, win);
    XSetInputFocus(d, win, RevertToPointerRoot, CurrentTime);

    GLXContext ctx = glXCreateContext(d, vi, 0, 1);
    if (!ctx) { dbg("morphdemo: no GLX context\n"); return 1; }
    glXMakeCurrent(d, win, ctx);

    GLuint prog = make_prog();
    if (!prog) return 1;
    glUseProgram(prog);
    GLint u_t = glGetUniformLocation(prog, "u_t");
    if (u_t < 0) { dbg("morphdemo: no u_t uniform\n"); return 1; }

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = RATE;
    ss.channels = 2;
    pa_buffer_attr ba;
    ba.maxlength = RATE * 4;
    ba.tlength = (unsigned int)(RATE * 4 * 0.20);   /* headroom: keeps the
                                                     * write non-blocking so
                                                     * the loop stays at the
                                                     * display refresh rate */
    ba.prebuf = 2048;
    ba.minreq = 1024;
    ba.fragsize = (unsigned int)-1;
    int aerr = 0;
    pa_simple *pa = pa_simple_new(0, "morphdemo", PA_STREAM_PLAYBACK, 0,
                                  "morphdemo", &ss, 0, &ba, &aerr);
    if (!pa)
        dbg("morphdemo: audio open failed, running without sound\n");

    KeyCode esc = XKeysymToKeycode(d, XK_Escape);
    static short sbuf[4096 * 2];
    double t0 = now_s();
    double taudio = 0.0;
    int quit = 0;

    while (!quit) {
        double t = now_s() - t0;
        if (t >= DUR) break;

        XEvent ev;
        while (XCheckWindowEvent(d, win, KeyPressMask, &ev)) {
            if (ev.type == KeyPress && ev.xkey.keycode == esc) { quit = 1; break; }
        }
        if (quit) break;

        double tf = now_s() - t0;
        glUniform1f(u_t, (float)tf);
        glBegin(GL_TRIANGLES);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(3.0f, -1.0f);
        glVertex2f(-1.0f, 3.0f);
        glEnd();
        glXSwapBuffers(d, win);

        /* push the audio that has come due; the loop runs at the display
         * refresh rate, so the score is written at exactly real-time rate */
        if (pa) {
            double due = t + ALAT - taudio;
            long n = (long)(due * RATE);
            if (n > 4096) n = 4096;
            if (n >= 256) {
                for (long i = 0; i < n; i++) {
                    float s = gen((float)(taudio + (double)i / RATE));
                    short v = (short)(s * 32767.0f);
                    sbuf[2 * i] = v;
                    sbuf[2 * i + 1] = v;
                }
                if (pa_simple_write(pa, sbuf, (unsigned long)n * 4, &aerr) < 0) {
                    dbg("morphdemo: audio write failed\n");
                    break;
                }
                taudio += (double)n / RATE;
            }
        }

        /* if vsync is off, cap the frame rate to keep CPU usage sane */
        double el = now_s() - t0 - tf;
        if (el < 0.012)
            sleep_us((long)((0.012 - el) * 1e6));
    }

    dbg("morphdemo: done\n");
    return 0;
}
