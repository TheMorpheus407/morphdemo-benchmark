/* gl_core.h -- GL 3.x/4.x entry points used by the 16 KiB demo.
 *
 * A single shader program renders the whole scene, so only the program,
 * uniform and vertex array calls are needed.
 */
#ifndef MORPH_GL_CORE_H
#define MORPH_GL_CORE_H

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
    X(glBufferData, void, (GLenum, GLsizeiptr, const void *, GLenum))       \
    X(glEnableVertexAttribArray, void, (GLuint))                           \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei))                         \
    X(glEnable, void, (GLenum))                                             \
    X(glDisable, void, (GLenum))                                            \
    X(glBlendFunc, void, (GLenum, GLenum))                                  \
    X(glDepthFunc, void, (GLenum))                                          \
    X(glLineWidth, void, (GLfloat))

#endif /* MORPH_GL_CORE_H */
