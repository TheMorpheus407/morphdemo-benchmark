#ifndef MORPH_PLATFORM_H
#define MORPH_PLATFORM_H

#include <stdint.h>
#include <dlfcn.h>
#include <math.h>

/* SDL2 Constants */
#define SDL_INIT_AUDIO              0x00000010u
#define SDL_INIT_VIDEO              0x00000020u
#define SDL_WINDOW_OPENGL           0x00000002u
#define SDL_WINDOW_FULLSCREEN_DESK  0x00001001u
#define AUDIO_S16LSB                0x8010u
#define AUDIO_F32LSB                0x8120u
#define SDL_QUIT_EV                 0x0100u
#define SDL_KEYDOWN_EV              0x0300u
#define SDL_SCANCODE_ESC            41u
#define SDLK_ESC                    27u

typedef struct {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    void (*callback)(void *userdata, uint8_t *stream, int len);
    void *userdata;
} SDL_AudioSpec;

/* OpenGL Constants */
#define GL_TRIANGLES                0x0004u
#define GL_DEPTH_BUFFER_BIT         0x0100u
#define GL_LEQUAL                   0x0203u
#define GL_SRC_ALPHA                0x0302u
#define GL_ONE_MINUS_SRC_ALPHA      0x0303u
#define GL_ONE                      1u
#define GL_DEPTH_TEST               0x0B71u
#define GL_BLEND                    0x0BE2u
#define GL_TEXTURE_2D               0x0DE1u
#define GL_FLOAT                    0x1406u
#define GL_DEPTH_COMPONENT          0x1902u
#define GL_RGBA                     0x1908u
#define GL_LINEAR                   0x2601u
#define GL_TEXTURE_MAG_FILTER       0x2800u
#define GL_TEXTURE_MIN_FILTER       0x2801u
#define GL_TEXTURE_WRAP_S           0x2802u
#define GL_TEXTURE_WRAP_T           0x2803u
#define GL_REPEAT                   0x2901u
#define GL_COLOR_BUFFER_BIT         0x4000u
#define GL_CLAMP_TO_EDGE            0x812Fu
#define GL_DEPTH_COMPONENT24        0x81A6u
#define GL_TEXTURE0                 0x84C0u
#define GL_RGBA16F                  0x881Au
#define GL_FRAGMENT_SHADER          0x8B30u
#define GL_VERTEX_SHADER            0x8B31u
#define GL_COMPILE_STATUS           0x8B81u
#define GL_LINK_STATUS              0x8B82u
#define GL_COLOR_ATTACHMENT0        0x8CE0u
#define GL_DEPTH_ATTACHMENT         0x8D00u
#define GL_FRAMEBUFFER              0x8D40u

static inline void load_dlsyms(void *lib, const char *names, void **out) {
    while (*names) {
        *out++ = dlsym(lib, names);
        while (*names++) {}
    }
}

static inline void load_glsyms(void *(*get_proc)(const char *), const char *names, void **out) {
    while (*names) {
        *out++ = get_proc(names);
        while (*names++) {}
    }
}

static inline __attribute__((noreturn)) void sys_exit_zero(void) {
    __asm__ volatile(
        "mov $60, %%eax\n"
        "xor %%edi, %%edi\n"
        "syscall\n"
        ::: "rax", "rdi"
    );
    __builtin_unreachable();
}

#define DEMO_ENTRY(entry_fn) \
    __asm__( \
        ".section .text.startup,\"ax\",@progbits\n" \
        ".global _start\n" \
        "_start:\n" \
        "    and $-16, %rsp\n" \
        "    call " #entry_fn "\n" \
        ".previous\n" \
    );

#endif
