/* glinfo.c -- report the GL version, renderer and limits of the display.
 *
 * Usage: glinfo [display]
 * Uses the same legacy GLX path as the demos so the numbers describe exactly
 * what the productions get to see.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Colormap;
typedef int Bool;
typedef struct _Visual Visual;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window, root, subwindow;
    unsigned long time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef union {
    int type;
    XKeyEvent xkey;
    long pad[24];
} XEvent;

typedef struct {
    Visual *visual;
    unsigned long visualid;
    int screen;
    int depth;
    int class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

typedef struct __GLXcontextRec *GLXContext;

int main(int argc, char **argv) {
    Display *(*XOpenDisplay)(const char *) = dlsym(RTLD_DEFAULT, "XOpenDisplay");
    Window (*XDefaultRootWindow)(Display *) = dlsym(RTLD_DEFAULT, "XDefaultRootWindow");
    XVisualInfo *(*glXChooseVisual)(Display *, int, int *) = dlsym(RTLD_DEFAULT, "glXChooseVisual");
    GLXContext (*glXCreateContext)(Display *, XVisualInfo *, GLXContext, Bool) =
        dlsym(RTLD_DEFAULT, "glXCreateContext");
    Bool (*glXMakeCurrent)(Display *, XID, GLXContext) = dlsym(RTLD_DEFAULT, "glXMakeCurrent");
    int (*glXQueryVersion)(Display *, int *, int *) = dlsym(RTLD_DEFAULT, "glXQueryVersion");
    const unsigned char *(*glXGetProcAddress)(const unsigned char *) =
        dlsym(RTLD_DEFAULT, "glXGetProcAddress");
    const unsigned char *(*glXGetProcAddressARB)(const unsigned char *) =
        dlsym(RTLD_DEFAULT, "glXGetProcAddressARB");
    const unsigned char *(*glGetString)(unsigned) = dlsym(RTLD_DEFAULT, "glGetString");
    const unsigned char *(*glGetStringi)(unsigned, unsigned) = dlsym(RTLD_DEFAULT, "glGetStringi");
    void (*glGetIntegerv)(unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetIntegerv");
    int (*glGetShaderiv)(unsigned, unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetShaderiv");
    int (*glGetProgramiv)(unsigned, unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetProgramiv");
    void *(*glGetProcAddress)(const unsigned char *) = dlsym(RTLD_DEFAULT, "glGetProcAddress");

    Display *d;
    XVisualInfo *vi;
    GLXContext ctx;
    int attrs[] = { 4, 8, 8, 9, 8, 10, 8, 5, 1, 0 };
    int maj = 0, min = 0, n = 0, i, v[8];
    const unsigned char *s;

    d = XOpenDisplay(argc > 1 ? argv[1] : NULL);
    if (!d) { printf("no display\n"); return 1; }
    glXQueryVersion(d, &maj, &min);
    printf("GLX version %d.%d\n", maj, min);
    vi = glXChooseVisual(d, 0, attrs);
    if (!vi) { printf("no visual\n"); return 1; }
    printf("visual depth %d class %d\n", vi->depth, vi->class);
    ctx = glXCreateContext(d, vi, NULL, 1);
    if (!ctx) { printf("no context\n"); return 1; }
    glXMakeCurrent(d, XDefaultRootWindow(d), ctx);
    s = glGetString(0x1F02); printf("GL_VENDOR   : %s\n", s ? (const char *)s : "?");
    s = glGetString(0x1F01); printf("GL_RENDERER : %s\n", s ? (const char *)s : "?");
    s = glGetString(0x1F00); printf("GL_VERSION  : %s\n", s ? (const char *)s : "?");
    s = glGetString(0x8B8C); printf("GL_SHADING  : %s\n", s ? (const char *)s : "?");
    if (glGetIntegerv) {
        glGetIntegerv(0x821B, v); printf("GL_MAX_FRAGMENT_UNIFORM_VECTORS %d\n", v[0]);
        glGetIntegerv(0x886E, v); printf("GL_MAX_TEXTURE_IMAGE_UNITS     %d\n", v[0]);
        glGetIntegerv(0x8B4D, v); printf("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS %d\n", v[0]);
        glGetIntegerv(0x0D33, v); printf("GL_MAX_TEXTURE_SIZE            %d\n", v[0]);
    }
    if (glGetStringi && glGetProcAddress) {
        const unsigned char *(*gsi)(unsigned, unsigned) = (const unsigned char *(*)(unsigned, unsigned))
            (uintptr_t)glGetProcAddress((const unsigned char *)"glGetStringi");
        if (gsi) {
            n = 0;
            glGetIntegerv(0x821D, v); n = v[0];
            printf("extensions (%d): ", n);
            for (i = 0; i < n; i++) {
                const unsigned char *e = gsi(0x1F03, i);
                if (e) printf("%s ", e);
            }
            printf("\n");
        }
    }
    (void)glGetShaderiv; (void)glGetProgramiv; (void)glXGetProcAddressARB;
    return 0;
}
