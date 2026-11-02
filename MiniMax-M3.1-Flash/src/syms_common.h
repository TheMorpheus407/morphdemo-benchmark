/* syms_common.h -- every runtime entry point the 16 KiB and 64 KiB demos use.
 *
 * The environment has no development headers for X11, GLX, OpenGL or ALSA, so
 * the prototypes are written by hand (see x11min.h and glconst.h).  The names
 * live in a single blob and are resolved once at start up with dlsym(), which
 * keeps the binaries free of a PLT entry per imported symbol.
 *
 * The list is shared by the platform layer and the productions: the table
 * itself lives in platform.c, the productions only read it.
 */
#ifndef MORPH_SYMS_COMMON_H
#define MORPH_SYMS_COMMON_H

#include "x11min.h"
#include "glconst.h"

#define SYMS_ALL(X)                                                          \
/* ------------------------------------------------------------------ X11 --- */ \
    X(XOpenDisplay, Display *, (const char *))                              \
    X(XCloseDisplay, int, (Display *))                                       \
    X(XDefaultScreen, int, (Display *))                                      \
    X(XDefaultRootWindow, Window, (Display *))                               \
    X(XDisplayWidth, int, (Display *, int))                                  \
    X(XDisplayHeight, int, (Display *, int))                                 \
    X(XCreateWindow, Window, (Display *, Window, int, int, unsigned int,     \
                              unsigned int, unsigned int, int, unsigned int,  \
                              Visual *, unsigned long, XSetWindowAttributes *)) \
    X(XCreateColormap, Colormap, (Display *, Window, Visual *, int))        \
    X(XMapWindow, int, (Display *, Window))                                  \
    X(XStoreName, int, (Display *, Window, const char *))                    \
    X(XSetWMNormalHints, int, (Display *, Window, XSizeHints *))            \
    X(XSetWMProtocols, Status, (Display *, Window, Atom *, int))             \
    X(XInternAtom, Atom, (Display *, const char *, Bool))                    \
    X(XFree, int, (void *))                                                  \
    X(XSync, int, (Display *, Bool))                                         \
    X(XPending, int, (Display *))                                            \
    X(XNextEvent, int, (Display *, XEvent *))                                \
    X(XLookupKeysym, KeySym, (XKeyEvent *, int))                             \
/* ------------------------------------------------------------------ GLX --- */ \
    X(glXChooseVisual, XVisualInfo *, (Display *, int, int *))              \
    X(glXCreateContext, GLXContext, (Display *, XVisualInfo *, GLXContext, Bool)) \
    X(glXMakeCurrent, Bool, (Display *, GLXDrawable, GLXContext))           \
    X(glXSwapBuffers, void, (Display *, GLXDrawable))                       \
    X(glXDestroyContext, void, (Display *, GLXContext))                      \
/* ------------------------------------------------------------- OpenGL ----- */ \
    X(glClear, void, (GLbitfield))                                          \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat))             \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei))                    \
    X(glEnable, void, (GLenum))                                              \
    X(glDisable, void, (GLenum))                                             \
    X(glBlendFunc, void, (GLenum, GLenum))                                   \
    X(glDepthFunc, void, (GLenum))                                           \
    X(glDepthMask, void, (GLboolean))                                        \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei))                          \
    X(glGenVertexArrays, void, (GLsizei, GLuint *))                          \
    X(glBindVertexArray, void, (GLuint))                                     \
    X(glGenBuffers, void, (GLsizei, GLuint *))                               \
    X(glBindBuffer, void, (GLenum, GLuint))                                  \
    X(glBufferData, void, (GLenum, GLsizeiptr, const void *, GLenum))        \
    X(glEnableVertexAttribArray, void, (GLuint))                             \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean,       \
                                    GLsizei, const void *))                  \
    X(glCreateShader, GLuint, (GLenum))                                     \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar *const *,         \
                             const GLint *))                                 \
    X(glCompileShader, void, (GLuint))                                      \
    X(glGetShaderiv, void, (GLuint, GLenum, GLint *))                       \
    X(glGetShaderInfoLog, void, (GLuint, GLsizei, GLsizei *, GLchar *))      \
    X(glGetProgramiv, void, (GLuint, GLenum, GLint *))                       \
    X(glCreateProgram, GLuint, (void))                                      \
    X(glAttachShader, void, (GLuint, GLuint))                               \
    X(glLinkProgram, void, (GLuint))                                        \
    X(glUseProgram, void, (GLuint))                                         \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar *))                 \
    X(glUniform1f, void, (GLint, GLfloat))                                  \
    X(glUniform2f, void, (GLint, GLfloat, GLfloat))                         \
    X(glUniform3f, void, (GLint, GLfloat, GLfloat, GLfloat))                \
    X(glUniform4f, void, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))       \
    X(glUniform1i, void, (GLint, GLint))                                    \
    X(glUniform1fv, void, (GLint, GLsizei, const GLfloat *))                \
    X(glUniform2fv, void, (GLint, GLsizei, const GLfloat *))                \
    X(glUniformMatrix4fv, void, (GLint, GLsizei, GLboolean, const GLfloat *))\
    X(glGenTextures, void, (GLsizei, GLuint *))                              \
    X(glBindTexture, void, (GLenum, GLuint))                                 \
    X(glTexImage2D, void, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint,   \
                           GLenum, GLenum, const void *))                    \
    X(glTexParameteri, void, (GLenum, GLenum, GLint))                       \
    X(glDeleteTextures, void, (GLsizei, const GLuint *))                     \
    X(glActiveTexture, void, (GLenum))                                       \
    X(glGenFramebuffers, void, (GLsizei, GLuint *))                         \
    X(glBindFramebuffer, void, (GLenum, GLuint))                            \
    X(glFramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint))\
    X(glCheckFramebufferStatus, GLenum, (GLenum))                              \
    X(glDeleteFramebuffers, void, (GLsizei, const GLuint *))                 \
    X(glDrawBuffers, void, (GLsizei, const GLenum *))                        \
    X(glPointSize, void, (GLfloat))                                          \
    X(glReadPixels, void, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    X(glGetString, const GLubyte *, (GLenum))                                \
    X(glGetError, GLenum, (void))                                            \
    X(glGetIntegerv, void, (GLenum, GLint *))                                 \
    X(glFinish, void, (void))                                                  \
/* ---------------------------------------------------------------- ALSA --- */ \
    X(snd_pcm_open, int, (void **, const char *, int, int))                  \
    X(snd_pcm_set_params, int, (void *, int, int, unsigned int, unsigned int,\
                                int, unsigned int))                          \
    X(snd_pcm_writei, long, (void *, const void *, unsigned long))           \
    X(snd_pcm_prepare, int, (void *))                                        \
    X(snd_pcm_close, int, (void *))

#endif /* MORPH_SYMS_COMMON_H */
