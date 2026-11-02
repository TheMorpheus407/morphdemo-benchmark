/* gl_full.h -- GL 4.x entry points used by the 64 KiB demo.
 *
 * The flagship production advances a particle field with a fragment shader
 * that writes position and velocity into two floating point targets, draws
 * the particles as additive billboards and post processes the result through
 * a framebuffer chain (bright pass, separable blur, composite).
 */
#ifndef MORPH_GL_FULL_H
#define MORPH_GL_FULL_H

#define SYMS_GL(X)                                                         \
    X(glClear, void, (GLbitfield))                                         \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat))            \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei))                   \
    X(glCreateShader, GLuint, (GLenum))                                    \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(glCompileShader, void, (GLuint))                                     \
    X(glGetShaderiv, void, (GLuint, GLenum, GLint *))                      \
    X(glGetProgramiv, void, (GLuint, GLenum, GLint *))                     \
    X(glCreateProgram, GLuint, (void))                                     \
    X(glAttachShader, void, (GLuint, GLuint))                              \
    X(glLinkProgram, void, (GLuint))                                       \
    X(glUseProgram, void, (GLuint))                                        \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar *))                \
    X(glUniform1f, void, (GLint, GLfloat))                                 \
    X(glUniform1i, void, (GLint, GLint))                                   \
    X(glUniform2f, void, (GLint, GLfloat, GLfloat))                        \
    X(glUniform3f, void, (GLint, GLfloat, GLfloat, GLfloat))               \
    X(glUniform4f, void, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))      \
    X(glUniform1fv, void, (GLint, GLsizei, const GLfloat *))               \
    X(glGenVertexArrays, void, (GLsizei, GLuint *))                        \
    X(glBindVertexArray, void, (GLuint))                                   \
    X(glGenBuffers, void, (GLsizei, GLuint *))                             \
    X(glBindBuffer, void, (GLenum, GLuint))                                \
    X(glBindBufferBase, void, (GLenum, GLuint, GLuint))                    \
    X(glBufferData, void, (GLenum, GLsizeiptr, const void *, GLenum))       \
    X(glEnableVertexAttribArray, void, (GLuint))                           \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei))                         \
    X(glEnable, void, (GLenum))                                             \
    X(glDisable, void, (GLenum))                                            \
    X(glBlendFunc, void, (GLenum, GLenum))                                  \
    X(glDepthFunc, void, (GLenum))                                          \
    X(glDepthMask, void, (GLboolean))                                       \
    X(glLineWidth, void, (GLfloat))                                         \
    X(glPointSize, void, (GLfloat))                                         \
    X(glGenTextures, void, (GLsizei, GLuint *))                             \
    X(glBindTexture, void, (GLenum, GLuint))                                \
    X(glActiveTexture, void, (GLenum))                                      \
    X(glTexImage2D, void, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(glTexParameteri, void, (GLenum, GLenum, GLint))                       \
    X(glPixelStorei, void, (GLenum, GLint))                                 \
    X(glGenFramebuffers, void, (GLsizei, GLuint *))                         \
    X(glBindFramebuffer, void, (GLenum, GLuint))                            \
    X(glFramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(glCheckFramebufferStatus, GLenum, (GLenum))                           \
    X(glDispatchCompute, void, (GLuint, GLuint, GLuint))                    \
    X(glMemoryBarrier, void, (GLbitfield))                                  \
    X(glFinish, void, (void))

#endif /* MORPH_GL_FULL_H */
