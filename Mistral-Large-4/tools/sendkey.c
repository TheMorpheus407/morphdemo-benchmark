/* sendkey.c — verification tool: send an Escape KeyPress/KeyRelease to the
 * window whose WM_NAME matches argv[1] (default "MORPH"), using XSendEvent.
 * usage: sendkey [window-name]
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>

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
    const char *name = argc > 1 ? argv[1] : "MORPH";
    Display *d = XOpenDisplay(0);
    if (!d) { fprintf(stderr, "sendkey: cannot open display\n"); return 1; }
    Window w = find(d, RootWindow(d, DefaultScreen(d)), name);
    if (!w) { fprintf(stderr, "sendkey: window '%s' not found\n", name); return 1; }
    KeyCode kc = XKeysymToKeycode(d, XK_Escape);
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xkey.type = KeyPress;
    e.xkey.display = d;
    e.xkey.window = w;
    e.xkey.keycode = kc;
    e.xkey.same_screen = True;
    XSendEvent(d, w, False, KeyPressMask, &e);
    e.xkey.type = KeyRelease;
    XSendEvent(d, w, False, KeyReleaseMask, &e);
    XFlush(d);
    fprintf(stderr, "sendkey: Escape sent to '%s' (keycode %d)\n", name, kc);
    return 0;
}
