/* xsend.c -- send an escape key press to the demo window (test helper).
 *
 * Usage: xsend [display]
 * Finds every mapped top level window of the running demo and sends it a
 * synthetic key press with the key code of escape.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Colormap;
typedef int Bool;
typedef struct _Visual Visual;
typedef struct _Screen Screen;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    int depth;
    Visual *visual;
    Window root;
    int class;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    Colormap colormap;
    Bool map_installed;
    int map_state;
    long all_event_masks;
    long your_event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Screen *screen;
} XWindowAttributes;

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

static int (*XQueryTree)(Display *, Window, Window *, Window *, Window **, unsigned int *);
static int (*XGetWindowAttributes)(Display *, Window, XWindowAttributes *);
static int (*XSendEvent)(Display *, Window, Bool, long, XEvent *);
static int (*XFlush)(Display *);
static int hits;

static void scan(Display *d, Window parent, int depth) {
    Window root, par, *kids = 0;
    unsigned int n = 0, i;
    if (!XQueryTree(d, parent, &root, &par, &kids, &n)) return;
    for (i = 0; i < n; i++) {
        XWindowAttributes a;
        memset(&a, 0, sizeof a);
        if (XGetWindowAttributes(d, kids[i], &a) &&
            a.width == 1920 && a.height == 1080 && a.map_state == 2) {
            XEvent e;
            memset(&e, 0, sizeof e);
            e.type = 2;                           /* KeyPress */
            e.xkey.window = kids[i];
            e.xkey.root = root;
            e.xkey.keycode = 9;                   /* escape */
            e.xkey.send_event = 1;
            XSendEvent(d, kids[i], 1, 0, &e);
            hits++;
        }
        scan(d, kids[i], depth + 1);
    }
    if (kids) free(kids);
}

int main(int argc, char **argv) {
    Display *(*XOpenDisplay)(const char *) = dlsym(RTLD_DEFAULT, "XOpenDisplay");
    Window (*XDefaultRootWindow)(Display *) = dlsym(RTLD_DEFAULT, "XDefaultRootWindow");
    Display *d;
    XQueryTree = dlsym(RTLD_DEFAULT, "XQueryTree");
    XGetWindowAttributes = dlsym(RTLD_DEFAULT, "XGetWindowAttributes");
    XSendEvent = dlsym(RTLD_DEFAULT, "XSendEvent");
    XFlush = dlsym(RTLD_DEFAULT, "XFlush");
    d = XOpenDisplay(argc > 1 ? argv[1] : NULL);
    if (!d) { printf("no display\n"); return 1; }
    scan(d, XDefaultRootWindow(d), 0);
    XFlush(d);
    printf("escape sent to %d window(s)\n", hits);
    return hits ? 0 : 1;
}
