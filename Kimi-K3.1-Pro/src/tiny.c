#include "platform.h"
#include "shader.h"

#define RATE 48000
#define LAST 2880000
#define DS 12000

static unsigned demo_clock;

typedef struct {
    unsigned pos, rng, delay;
    float kick_env, kick_phase, noise_env;
    float bass_env, bass_phase, bass_inc;
    float arp_env, arp_phase, arp_inc, pad_a;
    float dl[DS];
} Synth;

static Synth s;

static float wave(float p) {
    p -= (int)p;
    if (p < 0) ++p;
    float v = p < .25f ? 4*p : (p < .75f ? 2-4*p : 4*p-4);
    return v * (1.5f - .5f*v*v);
}

static float rnd() {
    unsigned x = s.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s.rng = x;
    return (float)(x >> 8) * (1.f/8388608.f) - 1.f;
}

static float ramp(unsigned p, unsigned a, unsigned b) {
    if (p <= a) return 0;
    if (p >= b) return 1;
    return (float)(p-a)/(b-a);
}

static void audio(void *data, unsigned char *stream, int bytes) {
    float *out = (float *)stream;
    static const float roots[4] = {.00152950f,.00152950f,.00121396f,.00136263f};
    static const float ratios[8] = {4,8,5.99323f,9.51366f,4.75683f,11.98646f,7.12719f,8.97970f};
    for (int i = 0; i < bytes/8; ++i) {
        unsigned p = s.pos, bp = p % 22500, sp = p % 5625;
        unsigned bi = p / 22500, si = p / 5625;
        unsigned root = (bi >> 2) & 3;
        float order = ramp(p,384000,1152000);
        float drums = ramp(p,480000,1056000) * (1-ramp(p,2640000,2832000));
        float arp_gain = ramp(p,816000,1392000) * (1-ramp(p,2688000,LAST));
        float bass_gain = ramp(p,576000,1104000);
        if (!bp) {
            s.kick_phase = 0;
            s.kick_env = (p > 480000 || !(bi&3)) ? .48f*drums+.12f : 0;
            if (p > 864000 && (bi&1)) s.noise_env = .16f*drums;
        }
        if (!(p%11250)) {
            s.bass_phase = 0;
            s.bass_env = .32f*bass_gain;
            s.bass_inc = roots[root] * (((p/11250)&2) ? 1.498307f : 1);
        }
        if (!sp) {
            s.arp_phase = 0;
            s.arp_env = .19f*arp_gain;
            s.arp_inc = roots[root]*ratios[si&7];
            if (p > 912000 && (si&1)) s.noise_env = .04f*drums;
        }
        float n = rnd();
        s.kick_env *= .99965f;
        s.kick_phase += .00102f + .0022f*s.kick_env;
        s.noise_env *= .998f;
        s.bass_env *= .99991f;
        s.bass_phase += s.bass_inc;
        s.arp_env *= .99942f;
        s.arp_phase += s.arp_inc;
        s.pad_a += roots[root]*2;
        float kick = wave(s.kick_phase)*s.kick_env;
        float bass = wave(s.bass_phase)*s.bass_env;
        float arp = (wave(s.arp_phase)+.25f*wave(s.arp_phase*2))*s.arp_env;
        float pad = wave(s.pad_a)*.08f*ramp(p,240000,960000);
        float noise = n*(s.noise_env+.075f-.065f*order);
        float pan = (si&1) ? .28f : .72f;
        float left = kick+bass+noise+arp*(1-pan)+pad;
        float right = kick+bass+noise*.8+arp*pan+pad*.9f;
        unsigned d = s.delay;
        left += s.dl[d]*.18f;
        right += s.dl[d]*.22f;
        s.dl[d] = (left+right)*.16f+arp*.2f+s.dl[d]*.28f;
        if (++d == DS) d = 0;
        s.delay = d;
        float master = ramp(p,0,12000)*(1-ramp(p,2774400,LAST));
        left *= master; right *= master;
        out[i*2] = left/(1+(left<0?-left:left));
        out[i*2+1] = right/(1+(right<0?-right:right));
        ++s.pos;
    }
    __atomic_store_n(&demo_clock,s.pos,__ATOMIC_RELAXED);
}

static const char vertex[] = "#version 330\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
static void *g[14];
#define gl_CreateShader ((GLuint(*)(GLenum))g[0])
#define gl_ShaderSource ((void(*)(GLuint,GLsizei,const GLchar*const*,const GLint*))g[1])
#define gl_CompileShader ((void(*)(GLuint))g[2])
#define gl_CreateProgram ((GLuint(*)(void))g[3])
#define gl_AttachShader ((void(*)(GLuint,GLuint))g[4])
#define gl_LinkProgram ((void(*)(GLuint))g[5])
#define gl_UseProgram ((void(*)(GLuint))g[6])
#define gl_GetUniformLocation ((GLint(*)(GLuint,const GLchar*))g[7])
#define gl_Uniform1f ((void(*)(GLint,GLfloat))g[8])
#define gl_Uniform2f ((void(*)(GLint,GLfloat,GLfloat))g[9])
#define gl_DrawArrays ((void(*)(GLenum,GLint,GLsizei))g[10])
#define gl_Viewport ((void(*)(GLint,GLint,GLsizei,GLsizei))g[11])
#define gl_GenVertexArrays ((void(*)(GLsizei,GLuint*))g[12])
#define gl_BindVertexArray ((void(*)(GLuint))g[13])

int main() {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("MORPH",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window || !SDL_GL_CreateContext(window)) return 1;
    static const char names[] = "glCreateShader\0glShaderSource\0glCompileShader\0glCreateProgram\0glAttachShader\0glLinkProgram\0glUseProgram\0glGetUniformLocation\0glUniform1f\0glUniform2f\0glDrawArrays\0glViewport\0glGenVertexArrays\0glBindVertexArray";
    const char *name = names;
    for (int i = 0; i < 14; ++i) {
        g[i] = SDL_GL_GetProcAddress(name);
        while (*name++) {}
    }
    GLuint vao;
    gl_GenVertexArrays(1,&vao);gl_BindVertexArray(vao);
    GLuint program=gl_CreateProgram();
    const char *sources[2] = {vertex,fragment_source};
    for (int i = 0; i < 2; ++i) {
        GLuint shader = gl_CreateShader(GL_VERTEX_SHADER-i);
        gl_ShaderSource(shader,1,sources+i,0);
        gl_CompileShader(shader);
        gl_AttachShader(program,shader);
    }
    gl_LinkProgram(program);gl_UseProgram(program);
    GLint ut=gl_GetUniformLocation(program,"T"),ur=gl_GetUniformLocation(program,"R");
    gl_Viewport(0,0,1920,1080);gl_Uniform2f(ur,1920,1080);
    SDL_GL_SetSwapInterval(1);
    SDL_AudioSpec want={0},got;
    want.freq=RATE;want.format=AUDIO_F32LSB;want.channels=2;want.samples=1024;
    want.callback=(void(*)(void*,Uint8*,int))audio;
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(0,0,&want,&got,0);
    if (!device) return 1;
    s.rng=0x92d68ca2u;s.pos=0;demo_clock=0;
    SDL_PauseAudioDevice(device,0);
    unsigned running=1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type==SDL_QUIT_EVENT || (event.type==SDL_KEYDOWN && event.padding[20]==SDLK_ESCAPE)) running=0;
        unsigned clock=__atomic_load_n(&demo_clock,__ATOMIC_RELAXED);
        if (clock>=LAST) running=0;
        gl_Uniform1f(ut,(float)clock*(1.f/RATE));
        gl_DrawArrays(GL_TRIANGLES,0,3);
        SDL_GL_SwapWindow(window);
    }
    SDL_Quit();
    return 0;
}
