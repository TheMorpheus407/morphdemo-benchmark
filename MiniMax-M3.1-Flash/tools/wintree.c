/* wintree.c -- dump the window tree of a display (development helper). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;

extern Display *XOpenDisplay(const char *);
extern Window XDefaultRootWindow(Display *);
extern int XQueryTree(Display *, Window, Window *, Window *, Window **, unsigned int *);
extern int XGetGeometry(Display *, Window, Window *, int *, int *, unsigned int *, unsigned int *,
                        unsigned int *, unsigned int *);

static void scan(Display *d, Window w, int depth) {
    Window root, par, *kids = 0;
    unsigned int n = 0, i;
    Window r;
    int gx, gy;
    unsigned int gw = 0, gh = 0, gb = 0, gd = 0;
    if (XGetGeometry(d, w, &r, &gx, &gy, &gw, &gh, &gb, &gd))
        printf("%*s0x%lx %ux%u+%d+%d depth %u\n", depth * 2, "", (unsigned long)w, gw, gh, gx, gy, gd);
    else
        printf("%*s0x%lx (no geometry)\n", depth * 2, "", (unsigned long)w);
    if (!XQueryTree(d, w, &root, &par, &kids, &n)) return;
    for (i = 0; i < n; i++) scan(d, kids[i], depth + 1);
    if (kids) free(kids);
}

int main(int argc, char **argv) {
    Display *d = XOpenDisplay(argc > 1 ? argv[1] : NULL);
    if (!d) { printf("no display\n"); return 1; }
    scan(d, XDefaultRootWindow(d), 0);
    return 0;
}
