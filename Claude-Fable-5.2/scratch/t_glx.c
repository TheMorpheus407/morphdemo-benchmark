#include <stdio.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){
  Display*d=XOpenDisplay(0); if(!d){puts("no display");return 1;}
  int scr=DefaultScreen(d);
  printf("screen %dx%d\n",DisplayWidth(d,scr),DisplayHeight(d,scr));
  int eb,evb; printf("glx ext: %d\n",glXQueryExtension(d,&eb,&evb));
  printf("server vendor: %s\n", glXQueryServerString(d,scr,GLX_VENDOR));
  printf("client vendor: %s\n", glXGetClientString(d,GLX_VENDOR));
  int att[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,None};
  XVisualInfo*vi=glXChooseVisual(d,scr,att); if(!vi){puts("no visual");return 1;}
  XSetWindowAttributes swa; memset(&swa,0,sizeof swa);
  swa.colormap=XCreateColormap(d,RootWindow(d,scr),vi->visual,AllocNone);
  swa.event_mask=KeyPressMask;
  Window w=XCreateWindow(d,RootWindow(d,scr),0,0,1920,1080,0,vi->depth,InputOutput,vi->visual,CWColormap|CWEventMask,&swa);
  XMapWindow(d,w);
  GLXContext c=glXCreateContext(d,vi,0,True); if(!c){puts("no ctx");return 1;}
  printf("makecurrent %d direct %d\n",glXMakeCurrent(d,w,c),glXIsDirect(d,c));
  printf("GL_VENDOR: %s\nGL_RENDERER: %s\nGL_VERSION: %s\nGLSL: %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION),glGetString(GL_SHADING_LANGUAGE_VERSION));
  double t0=now(); int n=0;
  while(now()-t0<2.0){glClearColor(n&1,0.5,0,1);glClear(GL_COLOR_BUFFER_BIT);glXSwapBuffers(d,w);n++;}
  printf("clear+swap fps: %.1f\n",n/(now()-t0));
  return 0;
}
