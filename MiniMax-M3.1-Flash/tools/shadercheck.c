/* shadercheck.c -- compile a shader file and report the errors.
 *
 * Usage: shadercheck <file.frag> [file.vert]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef int Bool;
typedef struct _Visual Visual;

typedef struct {
    Visual *visual;
    unsigned long visualid;
    int screen, depth, class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size, bits_per_rgb;
} XVisualInfo;

typedef struct __GLXcontextRec *GLXContext;

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    long n;
    char *b;
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { perror("read"); exit(1); }
    b[n] = 0;
    fclose(f);
    return b;
}

int main(int argc, char **argv) {
    Display *(*XOpenDisplay)(const char *) = dlsym(RTLD_DEFAULT, "XOpenDisplay");
    Window (*XDefaultRootWindow)(Display *) = dlsym(RTLD_DEFAULT, "XDefaultRootWindow");
    XVisualInfo *(*glXChooseVisual)(Display *, int, int *) = dlsym(RTLD_DEFAULT, "glXChooseVisual");
    GLXContext (*glXCreateContext)(Display *, XVisualInfo *, GLXContext, Bool) =
        dlsym(RTLD_DEFAULT, "glXCreateContext");
    Bool (*glXMakeCurrent)(Display *, XID, GLXContext) = dlsym(RTLD_DEFAULT, "glXMakeCurrent");
    unsigned (*glCreateShader)(unsigned) = dlsym(RTLD_DEFAULT, "glCreateShader");
    void (*glShaderSource)(unsigned, int, const char *const *, const int *) =
        dlsym(RTLD_DEFAULT, "glShaderSource");
    void (*glCompileShader)(unsigned) = dlsym(RTLD_DEFAULT, "glCompileShader");
    void (*glGetShaderiv)(unsigned, unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetShaderiv");
    void (*glGetShaderInfoLog)(unsigned, int, int *, char *) =
        dlsym(RTLD_DEFAULT, "glGetShaderInfoLog");
    void (*glDeleteShader)(unsigned) = dlsym(RTLD_DEFAULT, "glDeleteShader");

    Display *d;
    XVisualInfo *vi;
    GLXContext ctx;
    int attrs[] = { 4, 8, 8, 9, 8, 10, 8, 5, 1, 0 };
    char *src;
    unsigned sh;
    int ok = 0;
    char log[8192];
    int n = 0;

    if (argc < 2) { fprintf(stderr, "usage: shadercheck <frag> [vert]\n"); return 1; }
    d = XOpenDisplay(NULL);
    if (!d) { printf("no display\n"); return 1; }
    vi = glXChooseVisual(d, 0, attrs);
    ctx = glXCreateContext(d, vi, NULL, 1);
    glXMakeCurrent(d, XDefaultRootWindow(d), ctx);

    src = slurp(argv[1]);
    sh = glCreateShader(0x8B30);        /* fragment shader */
    glShaderSource(sh, 1, (const char *const *)&src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, 0x8B81, &ok);
    glGetShaderInfoLog(sh, sizeof log, &n, log);
    if (!ok) { printf("FRAGMENT FAILED:\n%s\n", log); return 1; }
    printf("fragment ok\n");

    if (argc > 2) {
        char *vsrc = slurp(argv[2]);
        unsigned vs = glCreateShader(0x8B31);
        int vok = 0;
        glShaderSource(vs, 1, (const char *const *)&vsrc, NULL);
        glCompileShader(vs);
        glGetShaderiv(vs, 0x8B81, &vok);
        glGetShaderInfoLog(vs, sizeof log, &n, log);
        if (!vok) { printf("VERTEX FAILED:\n%s\n", log); return 1; }
        printf("vertex ok\n");
    }
    return 0;
}
