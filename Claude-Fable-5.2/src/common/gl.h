/* Minimal OpenGL / GLX / EGL / Xlib / PulseAudio declarations (no system
 * headers are needed to build the demos).                                     */
#ifndef GL_H
#define GL_H

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte, GLboolean;
typedef long GLsizeiptr, GLintptr;
typedef char GLchar;

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_POINTS 0
#define GL_LINES 1
#define GL_TRIANGLES 4
#define GL_TRIANGLE_STRIP 5
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_BGRA 0x80E1
#define GL_DEPTH_COMPONENT 0x1902
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE0 0x84C0
#define GL_R8 0x8229
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_RGBA8 0x8058
#define GL_RGBA16F 0x881A
#define GL_RGBA32F 0x8814
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_ARRAY_BUFFER 0x8892
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_COPY 0x88EA
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_SHADER_STORAGE_BARRIER_BIT 0x2000
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x20
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x8
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#define GL_TIME_ELAPSED 0x88BF
#define GL_QUERY_RESULT 0x8866
#define GL_FUNC_ADD 0x8006
#define GL_MAX 0x8008
#define GL_PACK_ALIGNMENT 0x0D05

/* ------------------------------------------------------------------ Xlib */
typedef struct _XDisplay Display;
typedef unsigned long XID, Window, Atom, Colormap, Pixmap, Cursor, VisualID, KeySym;
typedef struct _XGC *GC;
typedef struct { void *ext_data; VisualID visualid; int c_class; unsigned long red_mask, green_mask, blue_mask; int bits_per_rgb, map_entries; } Visual;
typedef struct { Visual *visual; VisualID visualid; int screen, depth, c_class; unsigned long red_mask, green_mask, blue_mask; int colormap_size, bits_per_rgb; } XVisualInfo;
typedef struct {
  Pixmap background_pixmap; unsigned long background_pixel; Pixmap border_pixmap; unsigned long border_pixel;
  int bit_gravity, win_gravity, backing_store; unsigned long backing_planes, backing_pixel; int save_under;
  long event_mask, do_not_propagate_mask; int override_redirect; Colormap colormap; Cursor cursor;
} XSetWindowAttributes;
typedef struct {
  int x, y, width, height, border_width, depth; Visual *visual; Window root; int c_class, bit_gravity, win_gravity, backing_store;
  unsigned long backing_planes, backing_pixel; int save_under; Colormap colormap; int map_installed, map_state;
  long all_event_masks, your_event_mask, do_not_propagate_mask; int override_redirect; void *screen;
} XWindowAttributes;
typedef struct { int type; unsigned long serial; int send_event; Display *display; Window window, root, subwindow; unsigned long time; int x, y, x_root, y_root; unsigned int state, keycode; int same_screen; } XKeyEvent;
typedef struct { int type; unsigned long serial; int send_event; Display *display; Window event, window; int x, y, width, height, border_width; Window above; int override_redirect; } XConfigureEvent;
typedef struct { int type; unsigned long serial; int send_event; Display *display; Window window; Atom message_type; int format; union { char b[20]; short s[10]; long l[5]; } data; } XClientMessageEvent;
typedef union { int type; XKeyEvent xkey; XConfigureEvent xconfigure; XClientMessageEvent xclient; long pad[24]; } XEvent;
typedef struct { unsigned long pixel; unsigned short red, green, blue; char flags, pad; } XColor;
typedef struct _XImage XImage;

#define KeyPress 2
#define MapNotify 19
#define ConfigureNotify 22
#define ClientMessage 33
#define KeyPressMask 1L
#define StructureNotifyMask (1L << 17)
#define CWBackPixel 2L
#define CWBorderPixel 8L
#define CWEventMask (1L << 11)
#define CWColormap (1L << 13)
#define XK_Escape 0xff1b

/* ------------------------------------------------------------- PulseAudio */
typedef struct { int format; u32 rate; u8 channels; } pa_sample_spec;
typedef struct { u32 maxlength, tlength, prebuf, minreq, fragsize; } pa_buffer_attr;

#endif
