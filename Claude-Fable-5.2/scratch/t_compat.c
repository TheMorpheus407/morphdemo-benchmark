#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#define E(msg) do{GLenum e=glGetError(); printf("%-40s err=0x%x\n",msg,e);}while(0)
int main(int argc,char**argv){
  int useattr=argc>1;
  PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy=gpd(0x31DD,EGL_DEFAULT_DISPLAY,NULL); eglInitialize(dpy,0,0); eglBindAPI(EGL_OPENGL_API);
  EGLint at[]={0x30FD/*PROFILE_MASK*/,2/*COMPAT*/,0x3098,4,0x30FB,6,EGL_NONE};
  EGLContext c=eglCreateContext(dpy,(EGLConfig)0,EGL_NO_CONTEXT,useattr?at:NULL); printf("ctx %p eglerr %x\n",c,eglGetError());
  printf("makecurrent %d\n",eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,c));
  GLint mask=0,flags=0; glGetIntegerv(0x9126,&mask); printf("version %s profile mask %d (1=core 2=compat)\n",glGetString(GL_VERSION),mask); E("getint");
  glBindTexture(GL_TEXTURE_2D,1); E("bindtex 1");
  glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,1920,1080); E("texstorage");
  glBindFramebuffer(GL_FRAMEBUFFER,1); E("bindfb 1");
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,1,0); E("fbtex");
  printf("fb status 0x%x (complete=0x8cd5)\n",glCheckFramebufferStatus(GL_FRAMEBUFFER));
  glViewport(0,0,1920,1080); E("viewport");
  const char*src="void main(){gl_FragColor=vec4(.5,.25,1,1);}";
  GLuint p=glCreateShaderProgramv(GL_FRAGMENT_SHADER,1,&src); E("createprog");
  glUseProgram(p); E("useprogram");
  glRecti(-1,-1,1,1); E("recti");
  float px[4]={0}; glReadPixels(100,100,1,1,GL_RGBA,GL_FLOAT,px); E("readpixels"); printf("pixel %g %g %g %g\n",px[0],px[1],px[2],px[3]);
  GLint db=0; glGetIntegerv(GL_DRAW_BUFFER,&db); printf("draw buffer 0x%x\n",db);
  return 0;
}
