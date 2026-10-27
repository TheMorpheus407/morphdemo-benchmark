#include "platform.h"
#include "audio.h"
#include <math.h>

#ifndef DEMO
#define DEMO 0
#endif

#include "shader.h"

SDL_atomic_t demo_clock;

static const char vertex_source[] =
    "#version 330 core\n"
    "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
    "gl_Position=vec4(p*2.-1.,0.,1.);}";

static GLuint shader(GLenum type, const char *source) {
    GLuint id = gl_CreateShader(type);
    gl_ShaderSource(id, 1, &source, NULL);
    gl_CompileShader(id);
    GLint okay = 0;
    gl_GetShaderiv(id, GL_COMPILE_STATUS, &okay);
    if (!okay) {
        char log[4096];
        gl_GetShaderInfoLog(id, sizeof(log), NULL, log);
        SDL_Log("Shader compilation failed: %s", log);
        return 0;
    }
    return id;
}

static GLuint program(void) {
    GLuint vs = shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) return 0;
    GLuint p = gl_CreateProgram();
    gl_AttachShader(p, vs);
    gl_AttachShader(p, fs);
    gl_LinkProgram(p);
    GLint okay = 0;
    gl_GetProgramiv(p, GL_LINK_STATUS, &okay);
    if (!okay) {
        char log[4096];
        gl_GetProgramInfoLog(p, sizeof(log), NULL, log);
        SDL_Log("Shader link failed: %s", log);
    }
    gl_DeleteShader(vs);
    gl_DeleteShader(fs);
    return okay ? p : 0;
}

static int escape(const SDL_Event *event) {
    if (event->type == SDL_QUIT_EVENT) return 1;
    if (event->type == SDL_KEYDOWN && event->padding[20] == SDLK_ESCAPE) return 1;
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow("MORPH — from noise to order", 0, 0, 1920, 1080,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        SDL_Log("Window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context || !load_gl()) {
        SDL_Log("OpenGL: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    GLuint vao = 0;
    gl_GenVertexArrays(1, &vao);
    gl_BindVertexArray(vao);
    GLuint p = program();
    if (!p) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    gl_UseProgram(p);
    GLint u_time = gl_GetUniformLocation(p, "T");
    GLint u_resolution = gl_GetUniformLocation(p, "R");
    GLint u_beat = gl_GetUniformLocation(p, "B");
    GLint u_pulse = gl_GetUniformLocation(p, "E");
    SDL_GL_SetSwapInterval(1);
    SDL_ShowCursor(0);
    SDL_AudioDeviceID audio = audio_open();
    if (!audio) {
        SDL_Log("Audio: %s", SDL_GetError());
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_AtomicSet(&demo_clock, 0);
    audio_resume(audio);
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (escape(&event)) running = 0;
        float t = (float)SDL_AtomicGet(&demo_clock) / AUDIO_RATE;
        if (t >= DEMO_SECONDS) running = 0;
        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        gl_Viewport(0, 0, width, height);
        gl_Uniform2f(u_resolution, (float)width, (float)height);
        gl_Uniform1f(u_time, t);
#if DEMO == 0
        float beat = t * 2.f;
#elif DEMO == 1
        float beat = t * 1.6f;
#else
        float beat = t * (32.f / 15.f);
#endif
        float pulse = expf(-8.f * (beat - floorf(beat)));
        gl_Uniform1f(u_beat, beat);
        gl_Uniform1f(u_pulse, pulse);
        gl_DrawArrays(GL_TRIANGLES, 0, 3);
        SDL_GL_SwapWindow(window);
        SDL_Delay(1);
    }
    SDL_CloseAudioDevice(audio);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_ShowCursor(1);
    SDL_Quit();
    return 0;
}
