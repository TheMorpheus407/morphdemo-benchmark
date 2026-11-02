/* pointtest.c -- minimal check: can this context draw GL_POINTS?
 *
 * Draws a few large points into an offscreen framebuffer, reads the centre
 * pixel back and prints it.  Used to find out whether point sprites work in
 * the compatibility profile of this driver.
 */
#include <stdio.h>
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

int main(void) {
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
    unsigned (*glCreateProgram)(void) = dlsym(RTLD_DEFAULT, "glCreateProgram");
    void (*glAttachShader)(unsigned, unsigned) = dlsym(RTLD_DEFAULT, "glAttachShader");
    void (*glLinkProgram)(unsigned) = dlsym(RTLD_DEFAULT, "glLinkProgram");
    void (*glUseProgram)(unsigned) = dlsym(RTLD_DEFAULT, "glUseProgram");
    void (*glDrawArrays)(unsigned, int, int) = dlsym(RTLD_DEFAULT, "glDrawArrays");
    void (*glViewport)(int, int, int, int) = dlsym(RTLD_DEFAULT, "glViewport");
    void (*glEnable)(unsigned) = dlsym(RTLD_DEFAULT, "glEnable");
    void (*glDisable)(unsigned) = dlsym(RTLD_DEFAULT, "glDisable");
    void (*glBlendFunc)(unsigned, unsigned) = dlsym(RTLD_DEFAULT, "glBlendFunc");
    void (*glReadPixels)(int, int, int, int, unsigned, unsigned, void *) =
        dlsym(RTLD_DEFAULT, "glReadPixels");
    unsigned (*glGetError)(void) = dlsym(RTLD_DEFAULT, "glGetError");
    void (*glGenTextures)(int, unsigned *) = dlsym(RTLD_DEFAULT, "glGenTextures");
    void (*glBindTexture)(unsigned, unsigned) = dlsym(RTLD_DEFAULT, "glBindTexture");
    void (*glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) =
        dlsym(RTLD_DEFAULT, "glTexImage2D");
    void (*glTexParameteri)(unsigned, unsigned, int) = dlsym(RTLD_DEFAULT, "glTexParameteri");
    void (*glGenFramebuffers)(int, unsigned *) = dlsym(RTLD_DEFAULT, "glGenFramebuffers");
    void (*glBindFramebuffer)(unsigned, unsigned) = dlsym(RTLD_DEFAULT, "glBindFramebuffer");
    void (*glFramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int) =
        dlsym(RTLD_DEFAULT, "glFramebufferTexture2D");
    unsigned (*glCheckFramebufferStatus)(unsigned) = dlsym(RTLD_DEFAULT, "glCheckFramebufferStatus");
    float (*glGetFloatv)(unsigned, float *) = dlsym(RTLD_DEFAULT, "glGetFloatv");

    static const char VS[] =
        "#version 330\n"
        "void main(){gl_Position=vec4(0.,0.,0.,1.);gl_PointSize=200.;}\n";
    static const char FS[] =
        "#version 330\n"
        "out vec4 o;\n"
        "void main(){vec2 d=gl_PointCoord-.5;float r2=dot(d,d);\n"
        "if(r2>.25)discard;\n"
        "o=vec4(1.,0.,0.,1.);}\n";

    Display *d;
    XVisualInfo *vi;
    GLXContext ctx;
    int attrs[] = { 4, 8, 8, 9, 8, 10, 8, 5, 1, 0 };
    unsigned vs, fs, p, tex = 0, fbo = 0;
    float px[4], range[2];
    int ok = 0;

    printf("symbols: glGetFloatv=%p glDrawArrays=%p\n", (void *)glGetFloatv, (void *)glDrawArrays);
    fflush(stdout);
    d = XOpenDisplay(NULL);
    if (!d) { printf("no display\n"); return 1; }
    vi = glXChooseVisual(d, 0, attrs);
    ctx = glXCreateContext(d, vi, NULL, 1);
    glXMakeCurrent(d, XDefaultRootWindow(d), ctx);

    if (glGetFloatv) {
        glGetFloatv(0x846D, range);      /* GL_ALIASED_POINT_SIZE_RANGE */
        printf("point size range: %g .. %g\n", range[0], range[1]);
        fflush(stdout);
    }

    vs = glCreateShader(0x8B31);
    glShaderSource(vs, 1, (const char *const *)&VS, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, 0x8B81, &ok);
    printf("vertex compiles: %d\n", ok);
    fs = glCreateShader(0x8B30);
    glShaderSource(fs, 1, (const char *const *)&FS, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, 0x8B81, &ok);
    printf("fragment compiles: %d\n", ok);
    p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glGetShaderiv(p, 0x8B82, &ok);
    printf("program links: %d\n", ok);
    glUseProgram(p);

    /* into an offscreen target */
    glGenTextures(1, &tex);
    glBindTexture(0x0DE1, tex);
    glTexImage2D(0x0DE1, 0, 0x8058, 64, 64, 0, 0x1908, 0x1401, (const void *)0);
    glTexParameteri(0x0DE1, 0x2801, 0x2600);
    glTexParameteri(0x0DE1, 0x2800, 0x2600);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(0x8D40, fbo);
    glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, tex, 0);
    printf("framebuffer status: %x (complete is %x)\n",
           glCheckFramebufferStatus(0x8D40), 0x8CD5);

    glViewport(0, 0, 64, 64);
    glDisable(0x0B71);          /* depth test */
    glEnable(0x0BE2);           /* blend */
    glBlendFunc(1, 1);          /* one, one */
    glEnable(0x8642);           /* program point size */
    glDrawArrays(0x0000, 0, 16);
    printf("error after points: %x\n", glGetError());

    glReadPixels(32, 32, 1, 1, 0x1908, 0x1406, px);
    printf("centre pixel of the framebuffer: %.3f %.3f %.3f\n", px[0], px[1], px[2]);

    /* and into the window */
    glBindFramebuffer(0x8D40, 0);
    glViewport(0, 0, 640, 480);
    glDrawArrays(0x0000, 0, 16);
    glReadPixels(320, 240, 1, 1, 0x1908, 0x1406, px);
    printf("centre pixel of the window: %.3f %.3f %.3f\n", px[0], px[1], px[2]);
    return 0;
}
