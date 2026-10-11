/* shot.c — verification tool: capture a window (by WM_NAME, default "MORPH")
 * to a PPM.  Capturing the XWayland root window fails with BadMatch on this
 * machine, so we capture the demo window itself.
 * usage: shot <out.ppm> [window-name]
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_abs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static Window find(Display *d, Window w, const char *name)
{
    char *nm = NULL;
    if (XFetchName(d, w, &nm) && nm) {
        int ok = strcmp(nm, name) == 0;
        XFree(nm);
        if (ok) return w;
    }
    Window r, pp, *ch = NULL;
    unsigned n = 0;
    if (XQueryTree(d, w, &r, &pp, &ch, &n)) {
        for (unsigned i = 0; i < n; i++) {
            Window f = find(d, ch[i], name);
            if (f) { XFree(ch); return f; }
        }
    }
    if (ch) XFree(ch);
    return 0;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "shot.ppm";
    const char *name = argc > 2 ? argv[2] : "MORPH";
    Display *d = XOpenDisplay(0);
    if (!d) { fprintf(stderr, "shot: cannot open display\n"); return 1; }
    Window w = find(d, RootWindow(d, DefaultScreen(d)), name);
    if (!w) { fprintf(stderr, "shot: window '%s' not found\n", name); return 1; }
    XWindowAttributes wa;
    XGetWindowAttributes(d, w, &wa);
    int wd = wa.width, ht = wa.height;
    XImage *im = XGetImage(d, w, 0, 0, wd, ht, AllPlanes, ZPixmap);
    if (!im) { fprintf(stderr, "shot: XGetImage failed\n"); return 1; }
    fprintf(stderr, "shot: captured at abs=%.6f\n", now_abs());
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "shot: cannot write %s\n", out); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", wd, ht);
    for (int j = 0; j < ht; j++)
        for (int i = 0; i < wd; i++) {
            unsigned long px = XGetPixel(im, i, j);
            unsigned char rgb[3] = { (unsigned char)((px >> 16) & 255),
                                     (unsigned char)((px >> 8) & 255),
                                     (unsigned char)(px & 255) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    XDestroyImage(im);
    XCloseDisplay(d);
    fprintf(stderr, "shot: wrote %s (%dx%d)\n", out, wd, ht);
    return 0;
}
