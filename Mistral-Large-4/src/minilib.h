/*
 * minilib.h — minimal x86-64 Linux ABI declarations for X11, GLX, OpenGL,
 * Xrandr and PulseAudio (simple API).  Declared by hand so the demos build
 * with nothing but gcc/ld and the runtime shared libraries that ship with
 * the system (libX11.so.6, libGL.so.1, libXrandr.so.2, libpulse-simple.so.0).
 * Struct layouts were taken from the official headers (xorgproto / libX11 /
 * libglvnd / libXrandr / pulseaudio).
 */
#ifndef MINILIB_H
#define MINILIB_H

/* ---------------- X11 (xorgproto X.h + libX11 Xlib.h/Xutil.h) ------------- */

typedef unsigned long XID;
typedef XID Window;
typedef XID Pixmap;
typedef XID Colormap;
typedef XID Cursor;
typedef XID Atom;
typedef XID Drawable;
typedef XID KeySym;
typedef unsigned long VisualID;
typedef unsigned long Time;
typedef int Bool;
typedef unsigned char KeyCode;
typedef struct _XDisplay Display;
typedef struct _Visual Visual;

typedef struct {
    Visual *visual;
    VisualID visualid;
    int screen;
    int depth;
    int class;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

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

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef union {
    int type;
    XKeyEvent xkey;
    long pad[24];               /* XEvent is 192 bytes on x86-64 */
} XEvent;

#define KeyPress        2
#define KeyPressMask    (1L << 0)
#define CWOverrideRedirect (1L << 9)
#define CWEventMask     (1L << 11)
#define CWColormap      (1L << 13)
#define AllocNone       0
#define InputOutput     1
#define RevertToPointerRoot 1
#define CurrentTime     0L
#define XK_Escape       0xff1b

#ifndef MINI
extern Display *XOpenDisplay(const char *);
extern Window XDefaultRootWindow(Display *);
extern Colormap XCreateColormap(Display *, Window, Visual *, int);
extern Window XCreateWindow(Display *, Window, int, int, unsigned int,
                            unsigned int, unsigned int, int, unsigned int,
                            Visual *, unsigned long, XSetWindowAttributes *);
extern int XStoreName(Display *, Window, const char *);
extern int XMapWindow(Display *, Window);
extern int XSetInputFocus(Display *, Window, int, Time);
extern int XCheckWindowEvent(Display *, Window, long, XEvent *);
extern KeyCode XKeysymToKeycode(Display *, KeySym);
#endif

/* ---------------- Xrandr (libXrandr, RANDR 1.5 monitor objects) ------------- */

typedef struct {
    Atom name;
    Bool primary;
    Bool automatic;
    int noutput;
    int x;
    int y;
    int width;
    int height;
    int mwidth;
    int mheight;
    XID *outputs;               /* RROutput * */
} XRRMonitorInfo;

#ifndef MINI
extern XRRMonitorInfo *XRRGetMonitors(Display *, Window, Bool, int *);
extern void XRRFreeMonitors(XRRMonitorInfo *);
#endif

/* ---------------- GLX (libglvnd GL/glx.h) ---------------- */

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;

#define GLX_RGBA        4
#define GLX_DOUBLEBUFFER 5
#define GLX_RED_SIZE    8
#define GLX_GREEN_SIZE  9
#define GLX_BLUE_SIZE   10
#define GLX_ALPHA_SIZE  11
#define GLX_DEPTH_SIZE  12

#ifndef MINI
extern XVisualInfo *glXChooseVisual(Display *, int, int *);
extern GLXContext glXCreateContext(Display *, XVisualInfo *, GLXContext, Bool);
extern Bool glXMakeCurrent(Display *, GLXDrawable, GLXContext);
extern void glXSwapBuffers(Display *, GLXDrawable);
#endif

/* ---------------- OpenGL (libglvnd GL/gl.h) ---------------- */

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef char GLchar;
typedef void GLvoid;

#define GL_TRIANGLES         0x0004
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_DEPTH_TEST        0x0B71
#define GL_FRAGMENT_SHADER   0x8B30
#define GL_COMPILE_STATUS    0x8B81
#define GL_LINK_STATUS       0x8B82

#ifndef MINI
extern void glBegin(GLenum);
extern void glEnd(void);
extern void glVertex2f(GLfloat, GLfloat);
extern GLuint glCreateShader(GLenum);
extern void glShaderSource(GLuint, GLsizei, const GLchar **, const GLint *);
extern void glCompileShader(GLuint);
extern void glGetShaderiv(GLuint, GLenum, GLint *);
extern void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *, GLchar *);
extern GLuint glCreateProgram(void);
extern void glAttachShader(GLuint, GLuint);
extern void glLinkProgram(GLuint);
extern void glGetProgramiv(GLuint, GLenum, GLint *);
extern void glUseProgram(GLuint);
extern GLint glGetUniformLocation(GLuint, const GLchar *);
extern void glUniform1f(GLint, GLfloat);
#endif

/* ---------------- PulseAudio simple API (libpulse-simple) ---------------- */

typedef struct pa_simple pa_simple;

typedef struct {
    int format;                 /* pa_sample_format_t (enum) */
    unsigned int rate;
    unsigned char channels;
} pa_sample_spec;

typedef struct {
    unsigned int maxlength;
    unsigned int tlength;
    unsigned int prebuf;
    unsigned int minreq;
    unsigned int fragsize;
} pa_buffer_attr;

#define PA_SAMPLE_S16LE    3
#define PA_STREAM_PLAYBACK 1

#ifndef MINI
extern pa_simple *pa_simple_new(const char *, const char *, int, const char *,
                                const char *, const pa_sample_spec *,
                                const void *, const pa_buffer_attr *, int *);
extern int pa_simple_write(pa_simple *, const void *, unsigned long, int *);
#endif

#ifdef MINI
/* MINI mode: every import is resolved at runtime with dlsym(RTLD_DEFAULT,..)
 * so the binary needs only ONE dynamic symbol (dlsym, from libc) instead of a
 * dynsym/rela.plt/plt/got entry per function — saves ~3 KB of ELF plumbing.
 * The DT_NEEDED entries for libX11/libGL/libXrandr/libpulse-simple must be
 * kept (link with -Wl,--no-as-needed) so their symbols are in the global
 * lookup scope.  The import names live in one string table; fp[] holds the
 * resolved addresses.  Build with -DDBG to also import the shader compile
 * check functions (glGetShaderiv/glGetShaderInfoLog/glGetProgramiv). */
extern void *dlsym(void *, const char *);
#ifdef DBG
#define NF 28
#else
#define NF 20
#endif
static void *fp[NF];
static const char NAMES[] =
    "XOpenDisplay\0XDefaultRootWindow\0XCreateColormap\0XCreateWindow\0"
    "XStoreName\0XMapWindow\0XSetInputFocus\0XCheckWindowEvent\0"
    "XRRGetMonitors\0"
    "glXChooseVisual\0glXCreateContext\0glXMakeCurrent\0glXSwapBuffers\0"
#ifdef DBG
    "glCreateShader\0glShaderSource\0glCompileShader\0glCreateProgram\0"
    "glAttachShader\0glLinkProgram\0glUseProgram\0glGetUniformLocation\0"
    "glUniform4fv\0glRects\0pa_simple_new\0pa_simple_write\0"
    "glGetShaderiv\0glGetShaderInfoLog\0glGetProgramiv\0"
#else
    /* one call compiles+links the fragment shader (GL 2.0) */
    "glCreateShaderProgramv\0glUseProgram\0glGetUniformLocation\0"
    "glUniform4fv\0glRects\0pa_simple_new\0pa_simple_write\0"
#endif
    ;
static int mini_resolve(void)
{
    const char *p = NAMES;
    for (int i = 0; i < NF; i++) {
        fp[i] = dlsym(0, p);
        if (!fp[i]) return 0;
        while (*p) p++;
        p++;
    }
    return 1;
}
#define XOpenDisplay         ((Display *(*)(const char *))fp[0])
#define XDefaultRootWindow   ((Window (*)(Display *))fp[1])
#define XCreateColormap      ((Colormap (*)(Display *, Window, Visual *, int))fp[2])
#define XCreateWindow        ((Window (*)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *))fp[3])
#define XStoreName           ((int (*)(Display *, Window, const char *))fp[4])
#define XMapWindow           ((int (*)(Display *, Window))fp[5])
#define XSetInputFocus       ((int (*)(Display *, Window, int, Time))fp[6])
#define XCheckWindowEvent    ((int (*)(Display *, Window, long, XEvent *))fp[7])
#define XRRGetMonitors       ((XRRMonitorInfo *(*)(Display *, Window, Bool, int *))fp[8])
#define glXChooseVisual      ((XVisualInfo *(*)(Display *, int, int *))fp[9])
#define glXCreateContext     ((GLXContext (*)(Display *, XVisualInfo *, GLXContext, Bool))fp[10])
#define glXMakeCurrent       ((Bool (*)(Display *, GLXDrawable, GLXContext))fp[11])
#define glXSwapBuffers       ((void (*)(Display *, GLXDrawable))fp[12])
#ifdef DBG
#define glCreateShader       ((GLuint (*)(GLenum))fp[13])
#define glShaderSource       ((void (*)(GLuint, GLsizei, const GLchar **, const GLint *))fp[14])
#define glCompileShader      ((void (*)(GLuint))fp[15])
#define glCreateProgram      ((GLuint (*)(void))fp[16])
#define glAttachShader       ((void (*)(GLuint, GLuint))fp[17])
#define glLinkProgram        ((void (*)(GLuint))fp[18])
#define glUseProgram         ((void (*)(GLuint))fp[19])
#define glGetUniformLocation ((GLint (*)(GLuint, const GLchar *))fp[20])
#define glUniform4fv         ((void (*)(GLint, GLsizei, const GLfloat *))fp[21])
#define glRects              ((void (*)(short, short, short, short))fp[22])
#define pa_simple_new        ((pa_simple *(*)(const char *, const char *, int, const char *, const char *, const pa_sample_spec *, const void *, const pa_buffer_attr *, int *))fp[23])
#define pa_simple_write      ((int (*)(pa_simple *, const void *, unsigned long, int *))fp[24])
#define glGetShaderiv        ((void (*)(GLuint, GLenum, GLint *))fp[25])
#define glGetShaderInfoLog   ((void (*)(GLuint, GLsizei, GLsizei *, GLchar *))fp[26])
#define glGetProgramiv       ((void (*)(GLuint, GLenum, GLint *))fp[27])
#else
#define glCreateShaderProgramv ((GLuint (*)(GLenum, GLsizei, const GLchar **))fp[13])
#define glUseProgram         ((void (*)(GLuint))fp[14])
#define glGetUniformLocation ((GLint (*)(GLuint, const GLchar *))fp[15])
#define glUniform4fv         ((void (*)(GLint, GLsizei, const GLfloat *))fp[16])
#define glRects              ((void (*)(short, short, short, short))fp[17])
#define pa_simple_new        ((pa_simple *(*)(const char *, const char *, int, const char *, const char *, const pa_sample_spec *, const void *, const pa_buffer_attr *, int *))fp[18])
#define pa_simple_write      ((int (*)(pa_simple *, const void *, unsigned long, int *))fp[19])
#endif
#endif /* MINI */

#endif /* MINILIB_H */
