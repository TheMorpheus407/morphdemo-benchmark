#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void try_display(const char*name, EGLDisplay dpy){
  printf("== %s: dpy=%p\n",name,dpy); if(!dpy){printf("  err %x\n",eglGetError());return;}
  EGLint ma,mi; if(!eglInitialize(dpy,&ma,&mi)){printf("  init failed %x\n",eglGetError());return;}
  printf("  EGL %d.%d vendor=%s\n",ma,mi,eglQueryString(dpy,EGL_VENDOR));
  eglBindAPI(EGL_OPENGL_API);
  EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
  EGLConfig cfg; EGLint n=0; if(!eglChooseConfig(dpy,ca,&cfg,1,&n)||n<1){printf("  no config %x\n",eglGetError());return;}
  EGLint pa[]={EGL_WIDTH,1920,EGL_HEIGHT,1080,EGL_NONE};
  EGLSurface s=eglCreatePbufferSurface(dpy,cfg,pa); printf("  pbuffer %p\n",s);
  EGLContext c=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,NULL); printf("  ctx %p err %x\n",c,eglGetError());
  if(!c)return;
  printf("  makecurrent %d\n",eglMakeCurrent(dpy,s,s,c));
  printf("  GL_VENDOR: %s\n  GL_RENDERER: %s\n  GL_VERSION: %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
  unsigned char*buf=malloc(1920*1080*4);
  double t0=now(); int k=0;
  while(now()-t0<1.0){glClearColor(k&1,0.5,0,1);glClear(GL_COLOR_BUFFER_BIT);glReadPixels(0,0,1920,1080,GL_BGRA,GL_UNSIGNED_BYTE,buf);k++;}
  printf("  clear+readpixels fps: %.1f  px=%d %d %d %d\n",k/(now()-t0),buf[0],buf[1],buf[2],buf[3]);
  eglMakeCurrent(dpy,0,0,0);
}
int main(int argc,char**argv){
  const char*ext=eglQueryString(EGL_NO_DISPLAY,EGL_EXTENSIONS);
  printf("client ext: %s\n",ext?ext:"(null)");
  int mode=argc>1?atoi(argv[1]):0;
  if(mode==0||mode==1) try_display("eglGetDisplay(DEFAULT)",eglGetDisplay(EGL_DEFAULT_DISPLAY));
  if(mode==0||mode==2){
    PFNEGLQUERYDEVICESEXTPROC qd=(void*)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
    PFNEGLQUERYDEVICESTRINGEXTPROC qs=(void*)eglGetProcAddress("eglQueryDeviceStringEXT");
    EGLDeviceEXT devs[16]; EGLint nd=0; if(qd&&qd(16,devs,&nd)){ printf("devices: %d\n",nd);
      for(int i=0;i<nd;i++){ const char*e=qs?qs(devs[i],EGL_EXTENSIONS):0; printf("dev %d ext: %s\n",i,e?e:"null");
        char nm[64]; sprintf(nm,"device %d",i); try_display(nm,gpd(EGL_PLATFORM_DEVICE_EXT,devs[i],NULL)); } }
  }
  if(mode==0||mode==3){
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
    try_display("surfaceless",gpd(0x31DD,EGL_DEFAULT_DISPLAY,NULL));
  }
  return 0;
}
