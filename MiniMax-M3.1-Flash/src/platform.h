/* platform.h -- shared runtime services for the MorphDemo binaries.
 *
 * Everything the productions need from the outside world lives behind this
 * small interface: a window, an OpenGL context, a sound device that is fed
 * from the clock of the production, and the keyboard.
 */
#ifndef MORPH_PLATFORM_H
#define MORPH_PLATFORM_H

#include "x11min.h"
#include "glconst.h"

extern Display   *g_dpy;    /* X connection */
extern Window     g_win;    /* top level window */
extern GLXContext g_ctx;    /* GL context */
extern int        g_width;  /* width of the drawable */
extern int        g_height; /* height of the drawable */
extern int        g_rate;   /* sample rate of the sound device */

/* Opens the display, creates the window and the context.  `w' and `h' are the
 * requested size; the window is never larger than the screen.  Returns 0 when
 * no usable display or visual is available. */
int    plat_init(int w, int h, const char *title);
void   plat_done(void);
void   plat_swap(void);

/* Processes the pending events.  Returns 1 when the production should stop
 * (escape key, or the window manager asked the window to close). */
int    plat_poll(void);

/* Monotonic wall clock, in seconds. */
double plat_now(void);

/* Sound device.  The device is the clock of the production: the number of
 * frames the device has accepted defines the time of the picture that is
 * drawn, so image and music cannot drift apart.  plat_audio_write blocks
 * until the device takes the data. */
int    plat_audio_open(int rate);
int    plat_audio_write(const float *interleaved, int frames);
void   plat_audio_close(void);

/* Compiles and links a vertex/fragment shader pair.  Returns 0 on error. */
GLuint plat_program(const char *vs, const char *fs);

/* Makes a program current and looks up a uniform inside it. */
void plat_use(GLuint prog);
int  plat_uni(const char *name);

#endif /* MORPH_PLATFORM_H */
