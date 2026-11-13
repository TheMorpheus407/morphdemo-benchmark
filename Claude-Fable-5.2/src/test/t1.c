/* Pipeline smoke test: fullscreen shader + sine beeps, 8 s. */
#include "../common/sys.h"
#include "../common/gl.h"
#include "../common/plat_imports.h"
IMP(GLuint, glCreateShader, (GLenum))
IMP(void, glShaderSource, (GLuint, GLsizei, const char **, const GLint *))
IMP(void, glCompileShader, (GLuint))
IMP(GLuint, glCreateProgram, (void))
IMP(void, glAttachShader, (GLuint, GLuint))
IMP(void, glLinkProgram, (GLuint))
IMP(void, glUseProgram, (GLuint))
IMP(GLint, glGetUniformLocation, (GLuint, const char *))
IMP(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
IMP(void, glDrawArrays, (GLenum, GLint, GLsizei))
IMP(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
IMP(void, glGenVertexArrays, (GLsizei, GLuint *))
IMP(void, glBindVertexArray, (GLuint))
LIB("libm.so.6")
IMPN(float, m_sin, "sinf", (float))
IMP_END
#include "../common/plat.c"

#define RW 1920
#define RH 1080
#define LEN 8.0
static const char *vs = "#version 430\nvoid main(){gl_Position=vec4(vec2(gl_VertexID&1,gl_VertexID>>1)*4.-1.,0,1);}";
static const char *fs = "#version 430\nuniform vec4 u;out vec4 o;void main(){vec2 p=(gl_FragCoord.xy*2.-u.yz)/u.z;"
  "float b=exp(-6.*fract(u.x*2.));float r=length(p)-.4-.1*b;float a=atan(p.y,p.x)+u.x;"
  "vec3 c=vec3(.5+.5*cos(a*3.+vec3(0,2,4)))*smoothstep(.02,0.,abs(r))+b*.3*vec3(1,.5,.2)*step(length(p),.2);"
  "c+=step(abs(p.x-1.5+2.*fract(u.x*.25)*1.5),.01);o=vec4(c,1);}";
static GLuint prog, fbo, tex, vao;
static float abuf[(int)(LEN * AU_RATE) * 2];

static void audio(float *out, int f0, int n) {
  for (int i = 0; i < n; i++) {
    float t = (f0 + i) / (float)AU_RATE, ph = t * 2.f - (int)(t * 2.f);   /* beep every 0.5 s */
    float e = ph < 0.2f ? (1.f - ph * 5.f) : 0.f;
    float v = 0.3f * e * m_sin(t * 6.2831853f * 660.f);
    out[2 * i] = v; out[2 * i + 1] = v;
  }
}

int cmain(void) {
  imp_load();
  if (!plat_init()) return 1;
  GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(v, 1, &vs, 0); glCompileShader(v);
  glShaderSource(f, 1, &fs, 0); glCompileShader(f);
  prog = glCreateProgram(); glAttachShader(prog, v); glAttachShader(prog, f); glLinkProgram(prog);
  glGenVertexArrays(1, &vao); glBindVertexArray(vao);
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, RW, RH);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  plat_audio_start(audio, abuf, (int)(LEN * AU_RATE));
  for (;;) {
    if (!plat_poll()) break;
    double t = plat_time();
    if (t >= LEN) break;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, RW, RH);
    glUseProgram(prog);
    glUniform4f(glGetUniformLocation(prog, "u"), (float)t, RW, RH, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    plat_present(fbo, RW, RH);
  }
  plat_shutdown();
  return 0;
}
