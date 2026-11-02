/* xcap.c -- capture the X screen into a PPM file.
 *
 * A development helper used to verify what the demos put on screen.
 * Build:  gcc -O2 -o xcap tools/xcap.c -lX11
 * Usage:  DISPLAY=:0 ./xcap out.ppm [width height]
 *
 * The XImage layout below is the one the installed libX11 uses; it was
 * verified against a live XGetImage() result.
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;

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
extern int XDisplayWidth(Display *, int);
extern int XDisplayHeight(Display *, int);
extern int XDefaultScreen(Display *);

#define AllPlanes (~0UL)
#define ZPixmap 2

int main(int argc, char **argv) {
    Display *dpy;
    Window root;
    XImage *img;
    int w = 1920, h = 1080;
    FILE *f;
    unsigned char *row;
    unsigned int x, y;

    if (argc < 2) { fprintf(stderr, "usage: xcap out.ppm [w h]\n"); return 1; }
    if (argc > 3) { w = atoi(argv[2]); h = atoi(argv[3]); }
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "xcap: no display\n"); return 1; }
    root = XDefaultRootWindow(dpy);
    img = XGetImage(dpy, root, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "xcap: XGetImage failed\n"); return 1; }
    f = fopen(argv[1], "wb");
    if (!f) { perror("xcap"); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    row = malloc((size_t)w * 3);
    for (y = 0; y < (unsigned)h; y++) {
        const unsigned char *src = img->data + (size_t)y * img->bytes_per_line;
        for (x = 0; x < (unsigned)w; x++) {
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
    fprintf(stderr, "xcap: wrote %s (%dx%d)\n", argv[1], w, h);
    return 0;
}
