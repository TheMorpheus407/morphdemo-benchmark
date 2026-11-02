/* gl_fixed.h -- GL 1.x entry points used by the 4 KiB demo.
 *
 * The smallest production drives a fixed function pipeline: vertex colours,
 * blending and a depth buffer are all it needs.  Camera and projection are
 * computed on the CPU, so no matrix entry points are required.
 */
#ifndef MORPH_GL_FIXED_H
#define MORPH_GL_FIXED_H

#define SYMS_GL(X)                                                         \
    X(glClear, void, (GLbitfield))                                         \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat))            \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei))                   \
    X(glBegin, void, (GLenum))                                              \
    X(glEnd, void, (void))                                                  \
    X(glVertex3f, void, (GLfloat, GLfloat, GLfloat))                        \
    X(glVertex2f, void, (GLfloat, GLfloat))                                  \
    X(glPointSize, void, (GLfloat))                        \
    X(glColor3f, void, (GLfloat, GLfloat, GLfloat))                        \
    X(glEnable, void, (GLenum))                                             \
    X(glDisable, void, (GLenum))                                            \
    X(glBlendFunc, void, (GLenum, GLenum))                                  \
    X(glDepthFunc, void, (GLenum))                                          \
    X(glLineWidth, void, (GLfloat))

#endif /* MORPH_GL_FIXED_H */
