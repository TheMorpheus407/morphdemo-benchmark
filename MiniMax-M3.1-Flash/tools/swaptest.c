/* swaptest.c -- how long does a buffer swap take?
 *
 * Creates a window of a given size, clears it and swaps, and reports the
 * average time per swap.
 *
 *   swaptest [width] [height] [count]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/x11min.h"
#include "../src/glconst.h"

extern Display *XOpenDisplay(const char *);
extern int XDefaultScreen(Display *);
extern Window XDefaultRootWindow(Display *);
extern Window XCreateWindow(Display *, Window, int, int, unsigned int, unsigned int,
                            unsigned int, int, unsigned int, Visual *, unsigned long,
                            XSetWindowAttributes *);
extern Colormap XCreateColormap(Display *, Window, Visual *, int);
extern int XMapWindow(Display *, Window);
extern int XSync(Display *, Bool);
extern int XFree(void *);

extern void glClear(GLbitfield);
extern void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
extern void glFinish(void);

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

#define GLX_RGBA 4
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_DEPTH_SIZE 12
#define GLX_DOUBLEBUFFER 5

int main(int argc, char **argv) {
    int w = argc > 1 ? atoi(argv[1]) : 1920;
    int h = argc > 2 ? atoi(argv[2]) : 1080;
    int count = argc > 3 ? atoi(argv[3]) : 60;
    Display *dpy;
    Window root, win;
    Colormap cmap;
    XSetWindowAttributes swa;
    XVisualInfo *vi;
    int attrs[] = { GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                    GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    void *ctx;
    double t0, t1;
    int i;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { printf("no display\n"); return 1; }
    root = XDefaultRootWindow(dpy);
    vi = glXChooseVisual(dpy, XDefaultScreen(dpy), attrs);
    if (!vi) { printf("no visual\n"); return 1; }
    cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    memset(&swa, 0, sizeof swa);
    swa.colormap = cmap;
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.override_redirect = True;
    win = XCreateWindow(dpy, root, 0, 0, (unsigned)w, (unsigned)h, 0, vi->depth,
                        InputOutput, vi->visual,
                        CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect, &swa);
    XMapWindow(dpy, win);
    XSync(dpy, False);
    ctx = glXCreateContext(dpy, vi, NULL, True);
    glXMakeCurrent(dpy, win, ctx);
    glClearColor(0.1f, 0.1f, 0.2f, 1.f);
    for (i = 0; i < 5; i++) { glClear(GL_COLOR_BUFFER_BIT); glXSwapBuffers(dpy, win); }
    glFinish();
    t0 = now();
    for (i = 0; i < count; i++) {
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    glFinish();
    t1 = now();
    printf("%dx%d: %.2f ms per swap over %d swaps\n", w, h, (t1 - t0) * 1000.0 / count, count);
    return 0;
}
