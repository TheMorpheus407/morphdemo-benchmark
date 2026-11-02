/* x11min.h -- minimal X11/GLX declarations.
 *
 * The build environment ships no X11 development headers, so the handful of
 * entry points and structures we need are declared here by hand.  The layouts
 * follow the classic X11 ABI (Xlib.h / Xutil.h) for LP64.
 */
#ifndef MORPH_X11MIN_H
#define MORPH_X11MIN_H

typedef struct _XDisplay Display;
typedef struct _XVisual Visual;
typedef struct _XGC *GC;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Colormap;
typedef XID Cursor;
typedef XID Pixmap;
typedef XID Font;
typedef XID KeySym;
typedef unsigned long Atom;
typedef unsigned long Time;
typedef unsigned long VisualID;
typedef int Bool;
typedef int Status;

#define True 1
#define False 0
#define AllocNone 0
#define AllocAll 1

#define None 0L
#define InputOutput 1
#define InputOnly 2
#define ParentRelative 1L
#define CopyFromParent 0L
#define PointerWindow 0L
#define InputFocus 1L
#define PointerRoot 1L

/* event masks */
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define EnterWindowMask (1L << 4)
#define LeaveWindowMask (1L << 5)
#define PointerMotionMask (1L << 6)
#define ButtonMotionMask (1L << 13)
#define ExposureMask (1L << 15)
#define StructureNotifyMask (1L << 17)
#define FocusChangeMask (1L << 21)

#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)

/* event types */
#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define EnterNotify 7
#define LeaveNotify 8
#define FocusIn 9
#define FocusOut 10
#define Expose 12
#define DestroyNotify 17
#define UnmapNotify 18
#define MapNotify 19
#define ConfigureNotify 22
#define ClientMessage 33

/* window attribute mask bits */
#define CWBackPixmap (1L << 0)
#define CWBackPixel (1L << 1)
#define CWBorderPixmap (1L << 2)
#define CWBorderPixel (1L << 3)
#define CWBitGravity (1L << 4)
#define CWWinGravity (1L << 5)
#define CWBackingStore (1L << 6)
#define CWBackingPlanes (1L << 7)
#define CWBackingPixel (1L << 8)
#define CWOverrideRedirect (1L << 9)
#define CWSaveUnder (1L << 10)
#define CWEventMask (1L << 11)
#define CWDoNotPropagate (1L << 12)
#define CWColormap (1L << 13)
#define CWCursor (1L << 14)

/* keyboard */
#define KeySymMask (1L << 0)
#define ShiftMask (1L << 0)
#define ControlMask (1L << 2)
#define Mod1Mask (1L << 3)
#define XK_Escape 0xFF1B

/* XSetWindowAttributes -- layout as in Xlib.h (LP64) */
typedef struct {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    long event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Colormap colormap;
    Cursor cursor;
} XSetWindowAttributes;

/* XSizeHints -- layout as in Xutil.h (LP64) */
typedef struct {
    long flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    int width_inc, height_inc;
    struct { int x; int y; } min_aspect, max_aspect;
    int base_width, base_height;
    int win_gravity;
} XSizeHints;

#define USPosition (1L << 0)
#define USSize (1L << 1)
#define PPosition (1L << 2)
#define PSize (1L << 3)
#define PMinSize (1L << 4)
#define PMaxSize (1L << 5)
#define PBaseSize (1L << 6)

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window, root, subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window event;
    Window window;
    int x, y;
    int width, height;
    int border_width;
    Window above;
    Bool override_redirect;
} XConfigureEvent;

typedef union {
    int type;
    XKeyEvent xkey;
    XClientEvent xclient;
    XConfigureEvent xconfigure;
    long pad[24];
} XEvent;

/* XVisualInfo -- layout as in Xutil.h (LP64) */
typedef struct {
    Visual *visual;
    VisualID visualid;
    int screen;
    int depth;
    int class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

typedef struct {
    Display *display;
    int screen;
    Visual *visual;
    VisualID visualid;
    int depth;
    int class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XWindowAttributes;

extern Display *XOpenDisplay(const char *);
extern int XCloseDisplay(Display *);
extern int XDefaultScreen(Display *);
extern Window XDefaultRootWindow(Display *);
extern Window XRootWindow(Display *, int);
extern int XDisplayWidth(Display *, int);
extern int XDisplayHeight(Display *, int);
extern unsigned long XBlackPixel(Display *, int);
extern Window XCreateWindow(Display *, Window, int, int, unsigned int, unsigned int,
                            unsigned int, int, unsigned int, Visual *, unsigned long,
                            XSetWindowAttributes *);
extern Window XCreateSimpleWindow(Display *, Window, int, int, unsigned int, unsigned int,
                                  unsigned int, unsigned long, unsigned long);
extern int XDestroyWindow(Display *, Window);
extern int XMapWindow(Display *, Window);
extern int XMapRaised(Display *, Window);
extern int XUnmapWindow(Display *, Window);
extern int XSelectInput(Display *, Window, long);
extern int XPending(Display *);
extern int XNextEvent(Display *, XEvent *);
extern int XEventsQueued(Display *, int);
extern int XStoreName(Display *, Window, const char *);
extern int XFlush(Display *);
extern int XSync(Display *, Bool);
extern int XFree(void *);
extern int XFreeColormap(Display *, Colormap);
extern Colormap XCreateColormap(Display *, Window, Visual *, int);
extern int XSetWindowColormap(Display *, Window, Colormap);
extern int XSetWMNormalHints(Display *, Window, XSizeHints *);
extern Status XSetWMProtocols(Display *, Window, Atom *, int);
extern Atom XInternAtom(Display *, const char *, Bool);
extern Status XSendEvent(Display *, Window, Bool, long, XEvent *);
extern KeySym XLookupKeysym(XKeyEvent *, int);
extern int XGetWindowAttributes(Display *, Window, XWindowAttributes *);
extern int XResizeWindow(Display *, Window, unsigned int, unsigned int);
extern int XMoveResizeWindow(Display *, Window, int, int, unsigned int, unsigned int);

/* ---------------------------------------------------------------- GLX ---- */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;
typedef XID GLXWindow;
typedef void (*GLXproc)(void);

#define GLX_USE_GL 1
#define GLX_RGBA 4
#define GLX_RGBA_BIT 0x00000001
#define GLX_WINDOW_BIT 0x00000001
#define GLX_X_RENDERABLE 0x8012
#define GLX_DRAWABLE_TYPE 0x8010
#define GLX_RENDER_TYPE 0x8011
#define GLX_DOUBLEBUFFER 5
#define GLX_DEPTH_SIZE 12
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_STENCIL_SIZE 13
#define GLX_SAMPLE_BUFFERS 100000
#define GLX_SAMPLES 100001
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

extern GLXFBConfig *glXChooseFBConfig(Display *, int, const int *, int *);
extern XVisualInfo *glXChooseVisual(Display *, int, int *);
extern XVisualInfo *glXGetVisualFromFBConfig(Display *, GLXFBConfig);
extern GLXContext glXCreateContext(Display *, XVisualInfo *, GLXContext, Bool);
extern Bool glXMakeCurrent(Display *, GLXDrawable, GLXContext);
extern void glXSwapBuffers(Display *, GLXDrawable);
extern void glXDestroyContext(Display *, GLXContext);
extern void glXDestroyContextAttribsARB(Display *, GLXContext);
extern void (*glXGetProcAddressARB(const unsigned char *))(void);
extern void *glXGetProcAddress(const unsigned char *);

#endif /* MORPH_X11MIN_H */
