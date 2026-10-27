#ifndef MORPH_PLATFORM_H
#define MORPH_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int SDL_bool;
typedef struct { int value; } SDL_atomic_t;

typedef struct SDL_Window SDL_Window;
typedef void *SDL_GLContext;
typedef Uint32 SDL_AudioDeviceID;
typedef Uint16 SDL_AudioFormat;

typedef struct {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    void (*callback)(void *, Uint8 *, int);
    void *userdata;
} SDL_AudioSpec;

typedef union {
    Uint32 type;
    Uint8 padding[56];
} SDL_Event;

#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_WINDOW_FULLSCREEN 0x00000001u
#define SDL_WINDOW_OPENGL 0x00000002u
#define SDL_WINDOW_FULLSCREEN_DESKTOP (SDL_WINDOW_FULLSCREEN | 0x00001000u)
#define SDL_QUIT_EVENT 0x100u
#define SDL_KEYDOWN 0x300u
#define SDLK_ESCAPE 27
#define AUDIO_F32LSB 0x8120u
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_GL_CONTEXT_MINOR_VERSION 18
#define SDL_GL_CONTEXT_PROFILE_MASK 21
#define SDL_GL_CONTEXT_PROFILE_CORE 1

extern int SDL_Init(Uint32);
extern void SDL_Quit(void);
extern const char *SDL_GetError(void);
extern void SDL_Log(const char *, ...);
extern SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, Uint32);
extern void SDL_DestroyWindow(SDL_Window *);
extern int SDL_GL_SetAttribute(int, int);
extern SDL_GLContext SDL_GL_CreateContext(SDL_Window *);
extern void SDL_GL_DeleteContext(SDL_GLContext);
extern int SDL_GL_SetSwapInterval(int);
extern void SDL_GL_SwapWindow(SDL_Window *);
extern void *SDL_GL_GetProcAddress(const char *);
extern void SDL_GL_GetDrawableSize(SDL_Window *, int *, int *);
extern int SDL_PollEvent(SDL_Event *);
extern void SDL_Delay(Uint32);
extern int SDL_ShowCursor(int);
extern SDL_AudioDeviceID SDL_OpenAudioDevice(const char *, int, const SDL_AudioSpec *, SDL_AudioSpec *, int);
extern void SDL_CloseAudioDevice(SDL_AudioDeviceID);
extern void SDL_PauseAudioDevice(SDL_AudioDeviceID, int);
extern int SDL_AtomicGet(SDL_atomic_t *);
extern int SDL_AtomicSet(SDL_atomic_t *, int);

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef float GLfloat;
typedef unsigned char GLboolean;

#define GL_TRIANGLES 0x0004
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_VERTEX_SHADER 0x8b31
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82
#define GL_INFO_LOG_LENGTH 0x8b84

static GLuint (*gl_CreateShader)(GLenum);
static void (*gl_ShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
static void (*gl_CompileShader)(GLuint);
static void (*gl_GetShaderiv)(GLuint, GLenum, GLint *);
static void (*gl_GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
static GLuint (*gl_CreateProgram)(void);
static void (*gl_AttachShader)(GLuint, GLuint);
static void (*gl_LinkProgram)(GLuint);
static void (*gl_GetProgramiv)(GLuint, GLenum, GLint *);
static void (*gl_GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
static void (*gl_UseProgram)(GLuint);
static void (*gl_DeleteShader)(GLuint);
static GLint (*gl_GetUniformLocation)(GLuint, const GLchar *);
static void (*gl_Uniform1f)(GLint, GLfloat);
static void (*gl_Uniform2f)(GLint, GLfloat, GLfloat);
static void (*gl_DrawArrays)(GLenum, GLint, GLsizei);
static void (*gl_Viewport)(GLint, GLint, GLsizei, GLsizei);
static void (*gl_GenVertexArrays)(GLsizei, GLuint *);
static void (*gl_BindVertexArray)(GLuint);

static inline int load_gl(void) {
#define GL_LOAD(name) do { *(void **)(&gl_##name) = SDL_GL_GetProcAddress("gl" #name); if (!gl_##name) return 0; } while (0)
    GL_LOAD(CreateShader);
    GL_LOAD(ShaderSource);
    GL_LOAD(CompileShader);
    GL_LOAD(GetShaderiv);
    GL_LOAD(GetShaderInfoLog);
    GL_LOAD(CreateProgram);
    GL_LOAD(AttachShader);
    GL_LOAD(LinkProgram);
    GL_LOAD(GetProgramiv);
    GL_LOAD(GetProgramInfoLog);
    GL_LOAD(UseProgram);
    GL_LOAD(DeleteShader);
    GL_LOAD(GetUniformLocation);
    GL_LOAD(Uniform1f);
    GL_LOAD(Uniform2f);
    GL_LOAD(DrawArrays);
    GL_LOAD(Viewport);
    GL_LOAD(GenVertexArrays);
    GL_LOAD(BindVertexArray);
#undef GL_LOAD
    return 1;
}

#endif
