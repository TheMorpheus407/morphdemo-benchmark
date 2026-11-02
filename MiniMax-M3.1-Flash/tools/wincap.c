/* wincap.c -- capture the demo window (not the whole desktop) into a PPM.
 *
 * Usage: wincap out.ppm [width height]
 * Finds the mapped 1920x1080 window of the running demo and grabs it
 * directly, which avoids whatever the window manager puts on top of it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef struct _Visual Visual;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    Window window, root, subwindow;
    unsigned long time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    int same_screen;
} XKeyEvent;

typedef union {
    int type;
    XKeyEvent xkey;
    long pad[24];
} XEvent;

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
    int save_under;
    unsigned long colormap;
    int map_installed;
    int map_state;
    long all_event_masks;
    long your_event_mask;
    long do_not_propagate_mask;
    int override_redirect;
    void *screen;
} XWindowAttributes;

typedef struct {
    int width, height;
    int off_x, off_y;
    char *data;
    int f1, f2, f3, f4;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    int pad;
    unsigned long red_mask, green_mask, blue_mask;
} XImage;

extern Display *XOpenDisplay(const char *);
extern Window XDefaultRootWindow(Display *);
extern XImage *XGetImage(Display *, Drawable, int, int, unsigned int, unsigned int,
                         unsigned long, int);
extern int XCloseDisplay(Display *);
extern int XQueryTree(Display *, Window, Window *, Window *, Window **, unsigned int *);
extern int XGetWindowAttributes(Display *, Window, XWindowAttributes *);
extern int XGetGeometry(Display *, Drawable, Window *, int *, int *, unsigned int *, unsigned int *,
                        unsigned int *, unsigned int *);

#define AllPlanes (~0UL)
#define ZPixmap 2

static Window found;

static void scan(Display *d, Window parent) {
    Window root, par, *kids = 0;
    unsigned int n = 0, i;
    if (!XQueryTree(d, parent, &root, &par, &kids, &n)) return;
    for (i = 0; i < n; i++) {
        Window r;
        int gx, gy;
        unsigned int gw = 0, gh = 0, gb = 0, gd = 0;
        if (XGetGeometry(d, kids[i], &r, &gx, &gy, &gw, &gh, &gb, &gd) && gd == 24) {
            if (gw == 1920 && gh == 1080) {
                found = kids[i];
                fprintf(stderr, "wincap: window 0x%lx %ux%u depth %u (demo)\n",
                        (unsigned long)found, gw, gh, gd);
                break;
            }
            if (!found && gw >= 640 && gh >= 360) {
                found = kids[i];
                fprintf(stderr, "wincap: window 0x%lx %ux%u depth %u (fallback)\n",
                        (unsigned long)found, gw, gh, gd);
            }
        }
        scan(d, kids[i]);
    }
    if (kids) free(kids);
}

int main(int argc, char **argv) {
    Display *dpy;
    Window win;
    XImage *img;
    FILE *f;
    unsigned char *row;
    unsigned int x, y;
    unsigned int w = 0, h = 0;

    if (argc < 2) { fprintf(stderr, "usage: wincap out.ppm\n"); return 1; }
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "wincap: no display\n"); return 1; }
    scan(dpy, XDefaultRootWindow(dpy));
    win = found ? found : XDefaultRootWindow(dpy);
    {
        Window r;
        int gx, gy;
        unsigned int gw = 0, gh = 0, gb = 0, gd = 0;
        if (!XGetGeometry(dpy, win, &r, &gx, &gy, &gw, &gh, &gb, &gd)) {
            fprintf(stderr, "wincap: XGetGeometry failed\n");
            return 1;
        }
        img = XGetImage(dpy, win, 0, 0, gw, gh, AllPlanes, ZPixmap);
    }
    if (!img) { fprintf(stderr, "wincap: XGetImage failed\n"); return 1; }
    w = img->width; h = img->height;
    f = fopen(argv[1], "wb");
    if (!f) { perror("wincap"); return 1; }
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    row = malloc((size_t)w * 3);
    for (y = 0; y < h; y++) {
        const unsigned char *src = img->data + (size_t)y * img->bytes_per_line;
        for (x = 0; x < w; x++) {
            unsigned long v = 0;
            const unsigned char *p = src + x * (img->bits_per_pixel / 8);
            if (img->bits_per_pixel == 32) v = *(const unsigned int *)p;
            else if (img->bits_per_pixel == 16) v = *(const unsigned short *)p;
            row[x * 3 + 0] = (unsigned char)((v & img->red_mask) >> 16);
            row[x * 3 + 1] = (unsigned char)((v & img->green_mask) >> 8);
            row[x * 3 + 2] = (unsigned char)((v & img->blue_mask) >> 0);
        }
        fwrite(row, 1, (size_t)w * 3, f);
    }
    free(row);
    fclose(f);
    XCloseDisplay(dpy);
    fprintf(stderr, "wincap: wrote %s (%ux%u)\n", argv[1], w, h);
    return 0;
}
