#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){
  Display*d=XOpenDisplay(0); int scr=DefaultScreen(d);
  int W=DisplayWidth(d,scr),H=DisplayHeight(d,scr);
  printf("depth %d  bigreq/max req: %ld %ld\n",DefaultDepth(d,scr),XExtendedMaxRequestSize(d),XMaxRequestSize(d));
  XSetWindowAttributes swa; memset(&swa,0,sizeof swa); swa.event_mask=KeyPressMask|StructureNotifyMask;
  Window w=XCreateWindow(d,RootWindow(d,scr),0,0,W,H,0,CopyFromParent,InputOutput,CopyFromParent,CWEventMask,&swa);
  XMapRaised(d,w); XSync(d,0);
  PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy=gpd(0x31DD,EGL_DEFAULT_DISPLAY,NULL); EGLint ma,mi; eglInitialize(dpy,&ma,&mi);
  eglBindAPI(EGL_OPENGL_API);
  EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
  EGLConfig cfg; EGLint n=0; eglChooseConfig(dpy,ca,&cfg,1,&n);
  EGLint pa[]={EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};
  EGLSurface s=eglCreatePbufferSurface(dpy,cfg,pa);
  EGLContext c=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,NULL);
  eglMakeCurrent(dpy,s,s,c);
  printf("GL_RENDERER: %s\n",glGetString(GL_RENDERER));
  unsigned char*buf=malloc(W*H*4);
  XImage*img=XCreateImage(d,DefaultVisual(d,scr),DefaultDepth(d,scr),ZPixmap,0,(char*)buf,W,H,32,0);
  GC gc=DefaultGC(d,scr);
  double t0=now(),tr=0,tp=0; int k=0;
  while(now()-t0<3.0){
    double a=now();
    glClearColor((k%60)/60.0,0.5,0.2,1);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);glScissor((k*16)%W,0,64,H);glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
    glReadPixels(0,0,W,H,GL_BGRA,GL_UNSIGNED_BYTE,buf);
    double b=now();
    XPutImage(d,w,gc,img,0,0,0,0,W,H); XSync(d,0);
    double e=now(); tr+=b-a; tp+=e-b; k++;
  }
  printf("frames %d fps %.1f  render+read %.2f ms  putimage+sync %.2f ms\n",k,k/(now()-t0),tr/k*1e3,tp/k*1e3);
  return 0;
}
