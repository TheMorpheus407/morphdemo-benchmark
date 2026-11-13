#include "synth.h"
#include "../../build/demo64k_shaders.h"

#define NUM_PARTICLES 18432

typedef struct {
    int   (*Init)(uint32_t);
    void *(*CreateWindow)(const char *, int, int, int, int, uint32_t);
    void *(*GL_CreateContext)(void *);
    void *(*GL_GetProcAddress)(const char *);
    int   (*GL_SetSwapInterval)(int);
    void  (*GL_GetDrawableSize)(void *, int *, int *);
    void  (*GL_SwapWindow)(void *);
    int   (*ShowCursor)(int);
    int   (*OpenAudio)(SDL_AudioSpec *, SDL_AudioSpec *);
    void  (*PauseAudio)(int);
    int   (*PollEvent)(void *);
    int   (*GetTicks)(void);
    void  (*Delay)(uint32_t);
    void  (*Quit)(void);
} SdlApi64;

typedef struct {
    void     (*Viewport)(int, int, int, int);
    void     (*Clear)(uint32_t);
    void     (*Enable)(uint32_t);
    void     (*Disable)(uint32_t);
    void     (*DepthFunc)(uint32_t);
    void     (*DepthMask)(uint8_t);
    void     (*BlendFunc)(uint32_t, uint32_t);
    uint32_t (*CreateProgram)(void);
    uint32_t (*CreateShader)(uint32_t);
    void     (*ShaderSource)(uint32_t, int, const char **, const int *);
    void     (*CompileShader)(uint32_t);
    void     (*AttachShader)(uint32_t, uint32_t);
    void     (*LinkProgram)(uint32_t);
    void     (*UseProgram)(uint32_t);
    void     (*Uniform4fv)(int, int, const float *);
    void     (*GenVertexArrays)(int, uint32_t *);
    void     (*BindVertexArray)(uint32_t);
    void     (*DrawArrays)(uint32_t, int, int);
    void     (*GenTextures)(int, uint32_t *);
    void     (*ActiveTexture)(uint32_t);
    void     (*BindTexture)(uint32_t, uint32_t);
    void     (*TexImage2D)(uint32_t, int, int, int, int, int, uint32_t, uint32_t, const void *);
    void     (*TexParameteri)(uint32_t, uint32_t, int);
    void     (*GenFramebuffers)(int, uint32_t *);
    void     (*BindFramebuffer)(uint32_t, uint32_t);
    void     (*FramebufferTexture2D)(uint32_t, uint32_t, uint32_t, uint32_t, int);
} GlApi64;

static const char sdl_names64[] =
    "SDL_Init\0"
    "SDL_CreateWindow\0"
    "SDL_GL_CreateContext\0"
    "SDL_GL_GetProcAddress\0"
    "SDL_GL_SetSwapInterval\0"
    "SDL_GL_GetDrawableSize\0"
    "SDL_GL_SwapWindow\0"
    "SDL_ShowCursor\0"
    "SDL_OpenAudio\0"
    "SDL_PauseAudio\0"
    "SDL_PollEvent\0"
    "SDL_GetTicks\0"
    "SDL_Delay\0"
    "SDL_Quit\0";

static const char gl_names64[] =
    "glViewport\0"
    "glClear\0"
    "glEnable\0"
    "glDisable\0"
    "glDepthFunc\0"
    "glDepthMask\0"
    "glBlendFunc\0"
    "glCreateProgram\0"
    "glCreateShader\0"
    "glShaderSource\0"
    "glCompileShader\0"
    "glAttachShader\0"
    "glLinkProgram\0"
    "glUseProgram\0"
    "glUniform4fv\0"
    "glGenVertexArrays\0"
    "glBindVertexArray\0"
    "glDrawArrays\0"
    "glGenTextures\0"
    "glActiveTexture\0"
    "glBindTexture\0"
    "glTexImage2D\0"
    "glTexParameteri\0"
    "glGenFramebuffers\0"
    "glBindFramebuffer\0"
    "glFramebufferTexture2D\0";

typedef struct {
    Synth64 syn;
    SdlApi64 sdl;
    GlApi64 gl;
    float u[24]; /* 6 * vec4 */
    uint32_t ev[16];
} Demo64State;

static Demo64State D;

static SDL_AudioSpec want_spec64 = {
    SR64, AUDIO_F32LSB, 2, 0, 512, 0, 0, synth64_cb, &D.syn
};

static inline float sstep(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static uint32_t build_program(GlApi64 *gl, const char *vs_src, const char *fs_src) {
    uint32_t vs = gl->CreateShader(GL_VERTEX_SHADER);
    gl->ShaderSource(vs, 1, &vs_src, 0);
    gl->CompileShader(vs);

    uint32_t fs = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(fs, 1, &fs_src, 0);
    gl->CompileShader(fs);

    uint32_t prog = gl->CreateProgram();
    gl->AttachShader(prog, vs);
    gl->AttachShader(prog, fs);
    gl->LinkProgram(prog);
    return prog;
}

static void setup_tex2d(GlApi64 *gl, uint32_t tex, int int_fmt, int w, int h, uint32_t fmt) {
    gl->BindTexture(GL_TEXTURE_2D, tex);
    gl->TexImage2D(GL_TEXTURE_2D, 0, int_fmt, w, h, 0, fmt, GL_FLOAT, 0);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void alloc_hdr_targets(GlApi64 *gl, uint32_t *tex, uint32_t *fbo, int w, int h) {
    int bw = w > 2 ? (w >> 1) : 1;
    int bh = h > 2 ? (h >> 1) : 1;

    /* tex[2] = HDR color, tex[3] = 24-bit Depth -> fbo[2] */
    setup_tex2d(gl, tex[2], GL_RGBA16F, w, h, GL_RGBA);
    setup_tex2d(gl, tex[3], GL_DEPTH_COMPONENT24, w, h, GL_DEPTH_COMPONENT);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo[2]);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[2], 0);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex[3], 0);

    /* tex[4..5] = Half-res Bloom ping-pong -> fbo[3..4] */
    for (int i = 0; i < 2; i++) {
        setup_tex2d(gl, tex[4 + i], GL_RGBA16F, bw, bh, GL_RGBA);
        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo[3 + i]);
        gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[4 + i], 0);
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void demo64k_main(void) {
    Demo64State *d = &D;
    synth64_init(&d->syn);

    load_dlsyms(dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_GLOBAL), sdl_names64, (void **)&d->sdl);

    d->sdl.Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    void *win = d->sdl.CreateWindow("", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESK);
    d->sdl.GL_CreateContext(win);
    d->sdl.GL_SetSwapInterval(1);
    d->sdl.ShowCursor(0);
    load_glsyms(d->sdl.GL_GetProcAddress, gl_names64, (void **)&d->gl);

    uint32_t vao = 0;
    d->gl.GenVertexArrays(1, &vao);
    d->gl.BindVertexArray(vao);

    uint32_t prog_sim   = build_program(&d->gl, SHADER_QUAD_VERT, SHADER_SIM_64K);
    uint32_t prog_scene = build_program(&d->gl, SHADER_QUAD_VERT, SHADER_SCENE_64K);
    uint32_t prog_part  = build_program(&d->gl, SHADER_PART_VERT, SHADER_PART_FRAG);
    uint32_t prog_bloom = build_program(&d->gl, SHADER_QUAD_VERT, SHADER_BLOOM_64K);
    uint32_t prog_comp  = build_program(&d->gl, SHADER_QUAD_VERT, SHADER_COMP_64K);

    int cur_w = 1920, cur_h = 1080;
    d->sdl.GL_GetDrawableSize(win, &cur_w, &cur_h);

    /* tex[0..1]: 512x512 sim, tex[2]: HDR scene, tex[3]: HDR depth, tex[4..5]: bloom */
    uint32_t tex[6], fbo[5];
    d->gl.GenTextures(6, tex);
    d->gl.GenFramebuffers(5, fbo);

    for (int i = 0; i < 2; i++) {
        setup_tex2d(&d->gl, tex[i], GL_RGBA16F, 512, 512, GL_RGBA);
        d->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        d->gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        d->gl.Viewport(0, 0, 512, 512);
        d->gl.Clear(GL_COLOR_BUFFER_BIT);
    }
    alloc_hdr_targets(&d->gl, tex, fbo, cur_w, cur_h);

    d->sdl.OpenAudio(&want_spec64, 0);
    d->sdl.PauseAudio(0);

    int t0 = d->sdl.GetTicks();
    int sim_ping = 0;

    for (;;) {
        int ft0 = d->sdl.GetTicks();
        while (d->sdl.PollEvent(d->ev)) {
            if (d->ev[0] == SDL_QUIT_EV || (d->ev[0] == SDL_KEYDOWN_EV && d->ev[4] == SDL_SCANCODE_ESC)) {
                goto done;
            }
        }

        float t = (float)(ft0 - t0) * 0.001f;
        if (t >= 60.0f) break;

        int dw = 1920, dh = 1080;
        d->sdl.GL_GetDrawableSize(win, &dw, &dh);
        if (dw != cur_w || dh != cur_h) {
            cur_w = dw;
            cur_h = dh;
            alloc_hdr_targets(&d->gl, tex, fbo, cur_w, cur_h);
        }

        /* 5-Act weights */
        float wSing    = 1.0f - sstep(10.0f, 15.0f, t);
        float wToMono  = sstep(10.5f, 15.0f, t);
        float wAstro   = sstep(23.5f, 28.0f, t);
        float wMono    = wToMono * (1.0f - wAstro);
        float wCitadel = sstep(37.2f, 42.5f, t);
        float wMorph   = sstep(45.0f, 50.0f, t);

        /* Collision-free 5-Act Camera Spline:
         * Act I   (0-11s):  Close orbit around Singularity
         * Act II  (11-24s): Low dramatic dolly down the Brutalist Monolith Canyon (|x| <= 0.85)
         * Act III (24-38s): Wide elevated hero sweep around the Chrono-Mechanical Astrolabe
         * Act IV  (38-46s): Grand approach down the sunlit Alabaster Colonnade
         * Act V   (46-60s): Monumental centered framing of the beveled MORPH Citadel
         */
        float a1 = t * 0.36f;
        float ro1x = sinf(a1) * 4.5f, ro1y = 0.25f + 0.35f * sinf(t * 0.4f), ro1z = cosf(a1) * 4.5f;

        float ro2x = sinf(t * 0.32f) * 0.85f;
        float ro2y = -0.32f + 0.65f * sstep(11.0f, 23.0f, t);
        float ro2z = 6.2f - 1.9f * sstep(11.0f, 23.5f, t);

        float a3 = (t - 31.0f) * 0.28f;
        float ro3x = sinf(a3) * 5.35f, ro3y = 0.95f + 0.35f * sinf(t * 0.3f), ro3z = cosf(a3) * 5.35f;

        float ro4x = sinf((t - 50.0f) * 0.20f) * lerpf(0.65f, 0.38f, wMorph);
        float ro4y = lerpf(0.58f, 0.12f, wMorph) + 0.05f * sinf(t * 0.25f);
        float ro4z = lerpf(6.55f, 5.25f, wMorph);

        float rox = lerpf(ro1x, ro2x, wToMono);
        float roy = lerpf(ro1y, ro2y, wToMono);
        float roz = lerpf(ro1z, ro2z, wToMono);

        rox = lerpf(rox, ro3x, wAstro);
        roy = lerpf(roy, ro3y, wAstro);
        roz = lerpf(roz, ro3z, wAstro);

        rox = lerpf(rox, ro4x, wCitadel);
        roy = lerpf(roy, ro4y, wCitadel);
        roz = lerpf(roz, ro4z, wCitadel);

        float tay = lerpf(0.32f, 0.04f, wMorph);

        /* Pack 6 vec4 uniforms */
        d->u[0]  = (float)cur_w;
        d->u[1]  = (float)cur_h;
        d->u[2]  = t;
        d->u[3]  = t * (128.0f / 60.0f); /* exact beat position at 128 BPM */

        d->u[4]  = d->syn.vis_kick;
        d->u[5]  = d->syn.vis_snr;
        d->u[6]  = d->syn.vis_arp;
        d->u[7]  = d->syn.vis_bass;

        d->u[8]  = rox;
        d->u[9]  = roy;
        d->u[10] = roz;
        d->u[11] = 1.48f; /* focal length */

        d->u[12] = 0.0f;
        d->u[13] = tay;
        d->u[14] = 0.0f;
        d->u[15] = d->syn.vis_lead;

        d->u[16] = wSing;
        d->u[17] = wMono;
        d->u[18] = wAstro;
        d->u[19] = wCitadel;

        d->u[20] = wMorph;
        d->u[21] = 0.0f;
        d->u[22] = d->syn.vis_flash;
        d->u[23] = d->syn.vis_pad;

        /* Pass 1: 512x512 Wave & Vorticity Simulation */
        d->gl.Disable(GL_DEPTH_TEST);
        d->gl.Disable(GL_BLEND);
        d->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[sim_ping]);
        d->gl.Viewport(0, 0, 512, 512);
        d->gl.ActiveTexture(GL_TEXTURE0);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[1 - sim_ping]);
        d->gl.UseProgram(prog_sim);
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, 3);

        /* Pass 2: HDR Raymarched Scene + Exact Hardware Depth into fbo[2] */
        d->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[2]);
        d->gl.Viewport(0, 0, cur_w, cur_h);
        d->gl.Enable(GL_DEPTH_TEST);
        d->gl.DepthFunc(GL_LEQUAL);
        d->gl.DepthMask(1);
        d->gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        d->gl.ActiveTexture(GL_TEXTURE0);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[sim_ping]);
        d->gl.UseProgram(prog_scene);
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, 3);

        /* Pass 3: 18,432 Depth-Occluded 3D Crystal Shards & Strange-Attractor Ribbons */
        d->gl.DepthMask(0);
        d->gl.Enable(GL_BLEND);
        d->gl.BlendFunc(GL_ONE, GL_ONE);
        d->gl.UseProgram(prog_part);
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, NUM_PARTICLES * 3);
        d->gl.Disable(GL_BLEND);
        d->gl.Disable(GL_DEPTH_TEST);
        d->gl.DepthMask(1);

        /* Pass 4a & 4b: Half-Res HDR Separable + Anamorphic Cinema Bloom */
        int bw = cur_w > 2 ? (cur_w >> 1) : 1;
        int bh = cur_h > 2 ? (cur_h >> 1) : 1;
        d->gl.Viewport(0, 0, bw, bh);
        d->gl.UseProgram(prog_bloom);

        d->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[3]);
        d->gl.ActiveTexture(GL_TEXTURE0);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[2]);
        d->u[20] = 1.0f; d->u[21] = 0.0f;
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, 3);

        d->gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[4]);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[4]);
        d->u[20] = 0.0f; d->u[21] = 1.0f;
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, 3);

        /* Pass 5: Final ACES Filmic Composite + Chromatic Dispersion + Film Grain */
        d->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        d->gl.Viewport(0, 0, cur_w, cur_h);
        d->gl.ActiveTexture(GL_TEXTURE0);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[2]);
        d->gl.ActiveTexture(GL_TEXTURE0 + 1u);
        d->gl.BindTexture(GL_TEXTURE_2D, tex[5]);
        d->u[20] = wMorph; d->u[21] = 0.0f;
        d->gl.UseProgram(prog_comp);
        d->gl.Uniform4fv(0, 6, d->u);
        d->gl.DrawArrays(GL_TRIANGLES, 0, 3);

        sim_ping ^= 1;
        d->sdl.GL_SwapWindow(win);

        if (d->sdl.GetTicks() - ft0 < 4) d->sdl.Delay(15u);
    }

done:
    d->sdl.Quit();
    sys_exit_zero();
}

DEMO_ENTRY(demo64k_main)
