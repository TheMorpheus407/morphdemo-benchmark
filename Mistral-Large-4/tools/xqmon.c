/* xqmon.c — print RANDR monitors via XRRGetMonitors (host test tool). */
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

int main(void)
{
    Display *d = XOpenDisplay(0);
    if (!d) { printf("no display\n"); return 1; }
    Window root = DefaultRootWindow(d);
    int n = 0;
    XRRMonitorInfo *mi = XRRGetMonitors(d, root, True, &n);
    printf("nmonitors=%d\n", n);
    for (int i = 0; i < n; i++)
        printf("  [%d] primary=%d auto=%d x=%d y=%d w=%d h=%d nout=%d\n",
               i, mi[i].primary, mi[i].automatic, mi[i].x, mi[i].y,
               mi[i].width, mi[i].height, mi[i].noutput);
    if (mi) XRRFreeMonitors(mi);
    XCloseDisplay(d);
    return 0;
}
