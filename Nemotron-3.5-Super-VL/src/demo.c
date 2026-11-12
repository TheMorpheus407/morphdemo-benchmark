/* morphdemo.c — FROM NOISE TO ORDER
 *
 * One parametric source building three real-time audiovisual demos:
 *   -DDEMO_CLASS=0  -> 4k   (-nostdlib, custom _start in start.S)
 *   -DDEMO_CLASS=1  -> 16k
 *   -DDEMO_CLASS=2  -> 64k
 *
 * Self-declared SDL2 + OpenGL API: no system headers are needed at build time.
 * Everything (geometry, shaders, audio, font) is generated at runtime.
 *
 * Theme: FROM NOISE TO ORDER — a particle cloud begins in chaotic noise,
 * coalesces into an ordered structure, and resolves into a climax spelling
 * MORPH.  Sound and image share one timeline; every musical beat pulses the
 * visuals.
 */

#if !defined(DEMO_CLASS)
#define DEMO_CLASS 2
#endif

/* ---------------- size-class configuration ---------------- */
#if DEMO_CLASS == 0      /* 4,096 bytes */
# define N_PART        1024
# define N_TEXT_QUAD   64
# define AUDIO_VOICES  1
# define CAM_SHAKE     0
#elif DEMO_CLASS == 1    /* 16,384 bytes */
# define N_PART        3072
# define N_TEXT_QUAD   128
# define AUDIO_VOICES  2
# define CAM_SHAKE     0
#else                    /* 65,536 bytes */
# define N_PART        16384
# define N_TEXT_QUAD   256
# define AUDIO_VOICES  3
# define CAM_SHAKE     1
#endif

/* ---------------- self-declared SDL2 API ---------------- */
typedef unsigned char      Uint8;
typedef unsigned short     Uint16;
typedef unsigned int       Uint32;
typedef unsigned long long Uint64;
typedef void              *SDL_GLContext;
typedef struct SDL_Window  SDL_Window;
typedef void             (*SDL_AudioCallback)(void *, Uint8 *, int);
typedef struct {
    int             freq;
    Uint16          format;
    Uint8           channels;
    Uint8           silence;
    Uint16          samples;
    Uint16          padding;
    Uint32          size;
    SDL_AudioCallback callback;
    void           *userdata;
} SDL_AudioSpec;

#define SDL_INIT_VIDEO         0x00000020u
#define SDL_INIT_AUDIO         0x00000010u
#define SDL_WINDOW_OPENGL      0x00000002u
#define SDL_WINDOW_BORDERLESS  0x00000200u
#define SDL_AUDIO_F32          0x8010u
#define SDL_SCANCODE_ESCAPE    41

extern int             SDL_Init(Uint32);
extern SDL_Window     *SDL_CreateWindow(const char *, int, int, int, int, Uint32);
extern SDL_GLContext   SDL_GL_CreateContext(SDL_Window *);
extern int             SDL_GL_MakeCurrent(SDL_Window *, SDL_GLContext);
extern int             SDL_GL_SetSwapInterval(int);
extern void            SDL_GL_SwapWindow(SDL_Window *);
extern void            SDL_PumpEvents(void);
extern const Uint8    *SDL_GetKeyboardState(int *);
extern int             SDL_OpenAudio(SDL_AudioSpec *, SDL_AudioSpec *);
extern void            SDL_PauseAudio(int);
extern void            SDL_Quit(void);
extern Uint64          SDL_GetPerformanceCounter(void);
extern Uint64          SDL_GetPerformanceFrequency(void);

/* ---------------- self-declared OpenGL API ---------------- */
typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef char          GLchar;
typedef float         GLfloat;
typedef unsigned int  GLbitfield;
typedef long          GLsizeiptr;
typedef long          GLintptr;
typedef unsigned char GLboolean;

#define GL_ARRAY_BUFFER          0x8892
#define GL_DYNAMIC_DRAW          0x88E8
#define GL_POINTS                0x0000
#define GL_TRIANGLES             0x0004
#define GL_COLOR_BUFFER_BIT      0x00004000
#define GL_BLEND                 0x0BE2
#define GL_ONE                   0x0001
#define GL_SRC_ALPHA             0x0302
#define GL_ONE_MINUS_SRC_ALPHA   0x0303
#define GL_PROGRAM_POINT_SIZE    0x8642
#define GL_VERTEX_SHADER         0x8B31
#define GL_FRAGMENT_SHADER       0x8B30
#define GL_FLOAT                 0x1406
#define GL_FALSE                 0

extern void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
extern void glClear(GLbitfield);
extern void glViewport(GLint, GLint, GLsizei, GLsizei);
extern void glUseProgram(GLuint);
extern GLuint glCreateProgram(void);
extern void glAttachShader(GLuint, GLuint);
extern void glLinkProgram(GLuint);
extern void glDeleteShader(GLuint);
extern GLuint glCreateShader(GLenum);
extern void glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *);
extern void glCompileShader(GLuint);
extern GLint  glGetUniformLocation(GLuint, const GLchar *);
extern void glUniform1f(GLint, GLfloat);
extern void glUniform2f(GLint, GLfloat, GLfloat);
extern void glUniform3f(GLint, GLfloat, GLfloat, GLfloat);
extern void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void glUniform1i(GLint, GLint);
extern void glGenBuffers(GLsizei, GLuint *);
extern void glBindBuffer(GLenum, GLuint);
extern void glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern void glBufferSubData(GLenum, GLintptr, GLsizeiptr, const void *);
extern void glGenVertexArrays(GLsizei, GLuint *);
extern void glBindVertexArray(GLuint);
extern void glEnableVertexAttribArray(GLuint);
extern void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
extern void glDrawArrays(GLenum, GLint, GLsizei);
extern void glEnable(GLenum);
extern void glBlendFunc(GLenum, GLenum);

/* ---------------- tiny math (no libm) ---------------- */
typedef struct { float x, y, z; } Vec3;

static unsigned int g_rng = 0x243F6A88u;

static float frand11(void){
    g_rng = g_rng * 1103515245u + 12345u;
    return (float)(g_rng >> 16) * (1.0f / 32768.0f) - 1.0f;
}
static float frand01(void){ return frand11() * 0.5f + 0.5f; }

static float mysin(float x){
    while (x >  3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + x2 * x2 / 120.0f - x2 * x2 * x2 / 5040.0f);
}

static float note_freq(int n){
    static const float semi[12] = {
        1.000000f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f,
        1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f};
    int rel = n - 69;
    int oct = rel >= 0 ? rel / 12 : -((-rel + 11) / 12);
    int sem = rel - oct * 12;
    float f = 440.0f * semi[sem];
    while (oct > 0){ f *= 2.0f; oct--; }
    while (oct < 0){ f *= 0.5f; oct++; }
    return f;
}

static float smoothstep(float t){
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t * t * (3 - 2 * t);
}

static Vec3 v3(float x, float y, float z){ Vec3 v; v.x=x; v.y=y; v.z=z; return v; }
static Vec3 vmul(Vec3 a, float s){ return v3(a.x*s, a.y*s, a.z*s); }
static Vec3 vmix(Vec3 a, Vec3 b, float t){ return v3(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t); }

/* ---------------- particles ---------------- */
static Vec3 g_pos[N_PART];
static Vec3 g_col[N_PART];
static Vec3 g_home[N_PART];
static Vec3 g_hcol[N_PART];
static unsigned int g_seed[N_PART];

static void particle_init(void){
    for (int i = 0; i < N_PART; i++){
        float fi = (float)i / (float)(N_PART);
        g_seed[i] = i * 2654435761u + 0x9E3779B9u;
        Vec3 h, c;
        int type = i % 3;
        if (type == 0){
            float a = fi * 6.28318530f;
            float r = mysin(a * 3.0f) * 0.5f + 0.5f;
            h = v3(mysin(a) * r * 5.0f, frand11() * 5.0f, mysin(a * 2.0f) * r * 5.0f);
            c = v3(0.6f, 0.4f, 0.9f);
        } else if (type == 1){
            float a = fi * 6.28318530f * 2.0f;
            h = v3(mysin(a) * 4.0f, (fi * 2.0f - 1.0f) * 4.0f, mysin(a * 1.3f) * 4.0f);
            c = v3(0.2f, 0.8f, 0.6f);
        } else {
            float lx = (fi * 4.0f - 2.0f) * 2.2f;
            h = v3(lx, frand11() * 1.5f, frand11() * 1.5f);
            c = v3(0.9f, 0.7f, 0.3f);
        }
        g_home[i] = h;
        g_hcol[i] = c;
        g_pos[i] = v3(frand11()*6, frand11()*6, frand11()*6);
        g_col[i] = v3(frand01(), frand01(), frand01());
    }
}

static Vec3 noise_vec(int i, double t){
    unsigned int s = g_seed[i] + (unsigned int)(t * 1000.0) * 1597334677u;
    s = s * 1103515245u + 12345u; float x = (float)(s >> 16) / 32768.0f - 1.0f;
    s = s * 1103515245u + 12345u; float y = (float)(s >> 16) / 32768.0f - 1.0f;
    s = s * 1103515245u + 12345u; float z = (float)(s >> 16) / 32768.0f - 1.0f;
    return v3(x, y, z);
}

/* ---------------- audio ---------------- */
static volatile double g_time;
static double g_audio_t = 0;

static int phase_of(double t);

typedef struct {
    float pha;   /* oscillator phase 0..1 */
    float freq;  /* cycles per sample */
    float amp;   /* slewed amplitude */
    float tgt;   /* target amplitude */
    float nxt;   /* next noise value */
    unsigned int nc;
    int   mode;  /* 0 saw, 1 square, 2 tri, 3 noise */
} Voice;

static Voice g_voice[AUDIO_VOICES];

static float osc_tick(Voice *v){
    v->amp += (v->tgt - v->amp) * 0.10f;
    if (v->amp < 0.0005f) v->amp = 0;
    v->pha += v->freq;
    while (v->pha >= 1.0f) v->pha -= 1.0f;
    while (v->pha < 0.0f)  v->pha += 1.0f;
    float p = v->pha * 2.0f - 1.0f;
    float ap = p < 0 ? -p : p;
    float osc;
    switch (v->mode){
        case 0: osc = p; break;
        case 1: osc = (p < 0) ? -1.0f : 1.0f; break;
        case 2: osc = 1.0f - 2.0f * ap; break;
        default:
            if (v->nc++ > 64){ v->nc = 0; v->nxt = frand11(); }
            osc = v->nxt; break;
    }
    return osc * v->amp;
}

static void audio_mix(void *user, Uint8 *stream, int len){
    int nframes = len / (4 * 2); /* float32 stereo */
    float *out = (float *)stream;
    double t = g_audio_t;
    int ph = phase_of(t);
    float master = (ph == 0) ? 0.3f : ((ph == 1) ? 0.55f : 0.85f);
    for (int i = 0; i < nframes; i++, t += 1.0 / 48000.0){
        double bt = t * 2.0;            /* 120 bpm */
        long long bi = (long long)bt;
        double frac = bt - (double)bi;
        float pulse = (frac < 0.03f) ? (1.0f - (float)(frac / 0.03f)) : 0.0f;
        float l = 0, r = 0;
        if (ph == 0){
            /* emergence: noise + sparse dissonant pulses */
            float nz = frand11() * 0.35f;
            long long b = (long long)bt;
            float bass = ((b % 4 == 0) && frac < 0.04f) ? 0.5f : 0.0f;
            float blip = ((b % 3 == 1) && frac < 0.02f) ? 0.3f : 0.0f;
            l = nz + bass + blip;
            r = nz * 0.7f + bass * 0.6f;
        } else if (ph == 1){
            /* transformation: arpeggiating square lead + bass */
            long long b = (long long)bt;
            int note = 57 + (int)(b % 8);
            g_voice[0].mode = 1;
            g_voice[0].freq = (float)(note_freq(note) / 48000.0);
            g_voice[0].tgt = (frac < 0.75f) ? 0.35f : 0.0f;
            float s = osc_tick(&g_voice[0]);
            l += s * 0.5f; r += s * 0.46f;
#if AUDIO_VOICES >= 2
            if (b % 4 == 0 && frac < 0.25f){
                g_voice[1].mode = 0;
                g_voice[1].freq = (float)(note_freq(36) / 48000.0);
                g_voice[1].tgt = 0.7f;
                float sb = osc_tick(&g_voice[1]);
                l += sb * 0.6f; r += sb * 0.6f;
            }
#endif
        } else {
            /* resolution: warm chord resolving to root, then fade */
            g_voice[0].mode = 2; g_voice[0].freq = (float)(note_freq(60) / 48000.0); g_voice[0].tgt = 0.45f;
            float s0 = osc_tick(&g_voice[0]);
            l += s0 * 0.5f; r += s0 * 0.46f;
#if AUDIO_VOICES >= 2
            g_voice[1].mode = 2; g_voice[1].freq = (float)(note_freq(64) / 48000.0); g_voice[1].tgt = 0.40f;
            float s1 = osc_tick(&g_voice[1]);
            l += s1 * 0.45f; r += s1 * 0.45f;
#endif
#if AUDIO_VOICES >= 3
            g_voice[2].mode = 2; g_voice[2].freq = (float)(note_freq(67) / 48000.0); g_voice[2].tgt = 0.40f;
            float s2 = osc_tick(&g_voice[2]);
            l += s2 * 0.4f; r += s2 * 0.4f;
#endif
            if (t > 58.0) master *= (float)((60.0 - t) / 2.0);
        }
        l = l * master + pulse * 0.2f;
        r = r * master + pulse * 0.2f;
        if (l > 1.0f) l = 1.0f; if (l < -1.0f) l = -1.0f;
        if (r > 1.0f) r = 1.0f; if (r < -1.0f) r = -1.0f;
        out[2*i]   = l * 0.5f;
        out[2*i+1] = r * 0.5f;
    }
    g_audio_t = t;
}

/* letter bitmaps: 5 letters x 5 rows, bit0 = column 0 (left) */
static const unsigned char font[5][5] __attribute__((section(".text"))) = {
    { 0b10001, 0b11011, 0b10101, 0b10001, 0b10001 }, /* M */
    { 0b01110, 0b10001, 0b10001, 0b10001, 0b01110 }, /* O */
    { 0b01111, 0b10001, 0b01111, 0b00101, 0b10001 }, /* R */
    { 0b01111, 0b10001, 0b01111, 0b00001, 0b00001 }, /* P */
    { 0b10001, 0b10001, 0b01110, 0b10001, 0b10001 }  /* H */
};

static int phase_of(double t){
    if (t < 15.0) return 0;
    if (t < 40.0) return 1;
    return 2;
}

/* ---------------- OpenGL shaders ---------------- */
static const char vs_particle[] __attribute__((section(".text"))) =
"#version 330 core\nlayout(location=0) in vec3 pos;\nlayout(location=1) in vec3 col;\nlayout(location=0) uniform float u_a;layout(location=1) uniform float u_c;layout(location=2) uniform float u_f;layout(location=3) uniform vec2 u_r;out vec3 v;\nvoid main(){float s=sin(u_a),c=cos(u_a);float x=pos.x*c+pos.z*s,z=-pos.x*s+pos.z*c+u_c;gl_Position=vec4(vec2(x,pos.y)*(u_f/z)*vec2(2.0/u_r.x,2.0/u_r.y),0,1);gl_PointSize=2.0+8.0/z;v=col;}\n";

static const char fs_particle[] __attribute__((section(".text"))) =
"#version 330 core\nin vec3 v;out vec4 f;\nvoid main(){float d=length(gl_PointCoord-vec2(0.5));float a=1.0-d*2.0;if(a<=0)discard;f=vec4(v,a*a*0.9);}\n";

static const char vs_text[] __attribute__((section(".text"))) =
"#version 330 core\nlayout(location=0) in vec2 p;\nlayout(location=0) uniform vec2 u_r;void main(){gl_Position=vec4(p*2.0/u_r-vec2(1.0),0,1);}\n";

static const char fs_text[] __attribute__((section(".text"))) =
"#version 330 core\nlayout(location=0) uniform vec4 c;out vec4 f;\nvoid main(){f=c;}\n";

/* ---------------- program / uniform wrappers ---------------- */
static GLuint compile_shader(GLenum type, const char *src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    return s;
}
static GLuint make_program(const char *vs, const char *fs){
    GLuint p = glCreateProgram();
    GLuint vs_ = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint fs_ = compile_shader(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, vs_);
    glAttachShader(p, fs_);
    glLinkProgram(p);
    glDeleteShader(vs_);
    glDeleteShader(fs_);
    return p;
}

struct PParticle { GLuint prog; };
static struct PParticle g_pp;

struct PText { GLuint prog; };
static struct PText g_pt;

/* text vertex buffer: 6 vertices (2 triangles) per quad */
static float g_tv[N_TEXT_QUAD * 6 * 2];
static int   g_tcount;

static void text_quad(float x, float y, float w, float h){
    if (g_tcount + 6 > N_TEXT_QUAD * 6) return;
    float *v = &g_tv[g_tcount * 2];
    v[0]=x;     v[1]=y;     g_tcount++;
    v[2]=x+w;   v[3]=y;     g_tcount++;
    v[4]=x+w;   v[5]=y+h;   g_tcount++;
    v[6]=x;     v[7]=y;     g_tcount++;
    v[8]=x+w;   v[9]=y+h;   g_tcount++;
    v[10]=x;    v[11]=y+h;  g_tcount++;
}

/* ---------------- main ---------------- */
int main(void){
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return 1;

    SDL_Window *win = SDL_CreateWindow("morphdemo", 0, 0, 1920, 1080,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
    if (!win) return 2;
    SDL_GLContext glc = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, glc);
    SDL_GL_SetSwapInterval(1);

    /* audio: 48000 Hz float32 stereo */
    SDL_AudioSpec as;
    as.freq = 48000; as.format = SDL_AUDIO_F32; as.channels = 2;
    as.silence = 0; as.samples = 1024; as.size = 0;
    as.callback = audio_mix; as.userdata = 0;
    SDL_OpenAudio(&as, &as);
    SDL_PauseAudio(0);

    glClearColor(0, 0, 0, 1);

    g_pp.prog = make_program(vs_particle, fs_particle);


    /* particle buffers: positions then colors */
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_pos) + sizeof(g_col), 0, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (const void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (const void *)sizeof(g_pos));
    glEnable(GL_PROGRAM_POINT_SIZE);

    /* text buffer */
    GLuint tvao, tvbo;
    glGenVertexArrays(1, &tvao);
    glGenBuffers(1, &tvbo);
    glBindVertexArray(tvao);
    glBindBuffer(GL_ARRAY_BUFFER, tvbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_tv), 0, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    particle_init();

    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 t0 = SDL_GetPerformanceCounter();
    double duration = 60.0;

    while (1){
        Uint64 now = SDL_GetPerformanceCounter();
        double t = (double)(now - t0) / (double)freq;
        if (t >= duration) break;
        SDL_PumpEvents();
        const Uint8 *ks = SDL_GetKeyboardState(0);
        if (ks && ks[SDL_SCANCODE_ESCAPE]) break;

        g_time = t;
        int ph = phase_of(t);
        float seg = (ph == 0) ? (float)(t / 15.0)
                 : (ph == 1) ? (float)((t - 15.0) / 25.0)
                             : (float)((t - 40.0) / 20.0);
        float s = smoothstep(seg);
        if (seg > 1.0f) s = 1.0f;
        float m, n;
        if (ph == 0){ m = 0; n = 1; }
        else if (ph == 1){ m = s; n = 1.0f - s; }
        else { m = 1; n = 0; }

        double bt = t * 2.0;
        long long bi = (long long)bt;
        double frac = bt - (double)bi;
        float pulse = (frac < 0.03f) ? (1.0f - (float)(frac / 0.03f)) : 0.0f;
        n += pulse * 0.4f;

        float angle = (float)(t * 0.25f) + mysin((float)(t * 0.5f)) * 0.5f;
        float camdist = (ph == 0) ? 12.0f : ((ph == 1) ? (10.0f - m * 3.0f) : 7.5f);
#if CAM_SHAKE
        angle += mysin((float)(t * 17.0f)) * pulse * 0.05f;
#endif

        for (int i = 0; i < N_PART; i++){
            Vec3 r = noise_vec(i, t);
            Vec3 target = v3(g_home[i].x * m + r.x * n,
                             g_home[i].y * m + r.y * n,
                             g_home[i].z * m + r.z * n);
            g_pos[i] = v3(g_pos[i].x + (target.x - g_pos[i].x) * 0.2f,
                          g_pos[i].y + (target.y - g_pos[i].y) * 0.2f,
                          g_pos[i].z + (target.z - g_pos[i].z) * 0.2f);
            Vec3 rc = v3(frand01(), frand01(), frand01());
            Vec3 c = v3(rc.x + (g_hcol[i].x - rc.x) * m,
                        rc.y + (g_hcol[i].y - rc.y) * m,
                        rc.z + (g_hcol[i].z - rc.z) * m);
            float lum = 0.5f + m * 0.5f + pulse * 0.5f;
            g_col[i] = v3(c.x * lum, c.y * lum, c.z * lum);
        }

        int w = 1920, h = 1080;
        float fscale = (float)h * 0.866f;

        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(g_pos), g_pos);
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(g_pos), sizeof(g_col), g_col);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(g_pp.prog);
        glUniform1f(0, angle);
        glUniform1f(1, camdist);
        glUniform1f(2, fscale);
        glUniform2f(3, (float)w, (float)h);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, N_PART);

        /* draw MORPH in the resolution phase */
        if (ph == 2){
            g_tcount = 0;
            float tf = smoothstep((float)(t - 40.0) / 20.0);
            float scale = 1.0f + tf * 3.0f;
            float alpha = tf;
            float cx = (float)w * 0.5f;
            float cy = (float)h * 0.5f;
            float cw = 14.0f * scale, ch = 20.0f * scale;
            float x = cx - cw * 2.5f;
            float y = cy - ch * 2.5f;
            for (int li = 0; li < 5; li++){
                float lx = x + li * cw * 1.05f;
                for (int row = 0; row < 5; row++){
                    unsigned char bits = font[li][row];
                    float ry = y + row * ch * 0.22f;
                    for (int col = 0; col < 5; col++){
                        if (bits & (1u << col)){
                            float qw = cw * 0.18f, qh = ch * 0.22f;
                            text_quad(lx + col * qw * 1.05f, ry, qw, qh);
                        }
                    }
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, tvbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, g_tcount * 2 * sizeof(float), g_tv);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(g_pt.prog);
            glUniform2f(0, (float)w, (float)h);
            glUniform4f(1, 0.95f, 0.9f, 0.4f, alpha * 0.95f);
            glBindVertexArray(tvao);
            glDrawArrays(GL_TRIANGLES, 0, g_tcount);
        }

        SDL_GL_SwapWindow(win);
    }

    SDL_Quit();
    return 0;
}