/* Development/verification tool (not part of the demos):
 *   avrec SECONDS FPS OUTPREFIX [shots "t1,t2,..."] [-- command args...]
 * Captures the X root window (MIT-SHM) and the default PulseAudio source with a
 * common wall clock, optionally launching a command first.  Writes
 *   PREFIX.video.mp4 (via ffmpeg pipe, half resolution), PREFIX.audio.raw (s16le 48k stereo),
 *   PREFIX.frames.csv (t, mean r,g,b, diff-to-previous), PREFIX.info (timing, exit status),
 *   PREFIX.shot_<t>.ppm full-resolution stills at the requested times (relative to launch).   */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <pulse/simple.h>
#include <pulse/error.h>

static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double T0; static volatile int stop_all; static const char*prefix; static double dur;

static Window find_win(Display*d,Window w,const char*name,int depth){
  char*nm=0; if(XFetchName(d,w,&nm)&&nm){int ok=!strcmp(nm,name);XFree(nm);if(ok)return w;}
  if(!strcmp(name,"@big")&&depth>0){XWindowAttributes wa; if(XGetWindowAttributes(d,w,&wa)&&wa.map_state==2&&wa.class==1&&wa.width>=1000&&wa.height>=500&&depth>=1){ /* prefer the deepest big window (client inside WM frame) */
      Window r,p,*c=0;unsigned n=0; Window f=0; if(depth<=6&&XQueryTree(d,w,&r,&p,&c,&n)){for(unsigned i=0;i<n&&!f;i++)f=find_win(d,c[i],name,depth+1); if(c)XFree(c);} return f?f:w;}}
  Window r,p,*c=0;unsigned n=0; if(depth>6||!XQueryTree(d,w,&r,&p,&c,&n))return 0;
  Window f=0; for(unsigned i=0;i<n&&!f;i++)f=find_win(d,c[i],name,depth+1); if(c)XFree(c); return f;
}
static int xerr(Display*d,XErrorEvent*e){return 0;}
static void*audio_thread(void*a){
  pa_sample_spec ss={PA_SAMPLE_S16LE,48000,2}; int err=0;
  pa_buffer_attr ba={(unsigned)-1,(unsigned)-1,(unsigned)-1,(unsigned)-1,48000*4/100};
  pa_simple*s=pa_simple_new(0,"avrec",PA_STREAM_RECORD,0,"avrec",&ss,0,&ba,&err);
  char fn[512]; snprintf(fn,sizeof fn,"%s.audio.raw",prefix); FILE*f=fopen(fn,"wb");
  snprintf(fn,sizeof fn,"%s.audio.info",prefix); FILE*fi=fopen(fn,"w");
  if(!s){fprintf(fi,"error %s\n",pa_strerror(err));fclose(fi);return 0;}
  short buf[480*2]; long frames=0; int first=1;
  while(!stop_all){
    if(pa_simple_read(s,buf,sizeof buf,&err)<0)break;
    double t=now(); double lat=pa_simple_get_latency(s,&err)/1e6;
    if(first){ /* wall time (relative to T0) of the first sample in the file */
      fprintf(fi,"first_sample_time %.6f\n",t-lat-480/48000.0-T0); fflush(fi); first=0; }
    fwrite(buf,1,sizeof buf,f); frames+=480;
  }
  fprintf(fi,"frames %ld\n",frames); fclose(fi); fclose(f); return 0;
}

int main(int argc,char**argv){
  if(argc<4){fprintf(stderr,"usage\n");return 2;}
  dur=atof(argv[1]); double fps=atof(argv[2]); prefix=argv[3];
  double shots[256]; int nshots=0; char**cmd=0; const char*winname=0;
  for(int i=4;i<argc;i++){
    if(!strcmp(argv[i],"--")){cmd=argv+i+1;break;}
    if(!strcmp(argv[i],"win")&&i+1<argc){winname=argv[++i];continue;}
    if(!strcmp(argv[i],"shots")&&i+1<argc){char*p=argv[++i];while(*p&&nshots<256){shots[nshots++]=strtod(p,&p);if(*p==',')p++;}}
  }
  Display*d=XOpenDisplay(0); if(!d){fprintf(stderr,"no display\n");return 1;}
  XSetErrorHandler(xerr); int scr=DefaultScreen(d); Window root=RootWindow(d,scr); Window src=root; int haswin=0; int W=DisplayWidth(d,scr),H=DisplayHeight(d,scr);
  T0=now();
  pthread_t th; pthread_create(&th,0,audio_thread,0);
  usleep(300000); /* let the recorder settle before launching */
  pid_t pid=0; double tl=now()-T0, texit=-1; int status=-1;
  if(cmd){ pid=fork(); if(!pid){execvp(cmd[0],cmd);_exit(127);} }
  char fn[512]; snprintf(fn,sizeof fn,"%s.info",prefix); FILE*fi=fopen(fn,"w"); fprintf(fi,"launch_time %.6f\n",tl);
  if(winname){ /* wait for the named window (rootless XWayland: the root window is not composited) */
    while(now()-T0<tl+8){ Window f=find_win(d,root,winname,0); XWindowAttributes wa;
      if(f&&XGetWindowAttributes(d,f,&wa)&&wa.map_state==2){usleep(700000);XGetWindowAttributes(d,f,&wa);src=f;haswin=1;W=wa.width;H=wa.height;fprintf(fi,"window 0x%lx %dx%d found at t=%.3f\n",f,W,H,now()-T0-tl);break;}
      usleep(20000);} }
  fprintf(fi,"size %dx%d\n",W,H); fflush(fi);
  XShmSegmentInfo si; XImage*img=XShmCreateImage(d,DefaultVisual(d,scr),DefaultDepth(d,scr),ZPixmap,0,&si,W,H);
  si.shmid=shmget(IPC_PRIVATE,img->bytes_per_line*img->height,IPC_CREAT|0600); si.shmaddr=img->data=shmat(si.shmid,0,0); si.readOnly=0;
  int useshm=XShmAttach(d,&si); XSync(d,0);
  snprintf(fn,sizeof fn,"%s.frames.csv",prefix); FILE*fc=fopen(fn,"w"); fprintf(fc,"t,r,g,b,diff\n");
  char ff[1024]; int hw=W/2,hh=H/2; if(W>2000){hw=W/4;hh=H/4;}
  int ds=W/hw;
  snprintf(ff,sizeof ff,"ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgr0 -s %dx%d -r %g -i - -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p %s.video.mp4",hw,hh,fps,prefix);
  FILE*fv=popen(ff,"w");
  unsigned char*half=malloc(hw*hh*4), *prev=calloc(hw*hh,1);
  long k=0; int shot_i=0;
  for(;;){
    double t=now()-T0; if(t>=dur)break;
    double tn=tl+k/fps; if(t<tn){usleep((useconds_t)((tn-t)*1e6));}
    t=now()-T0;
    if(useshm&&!winname) XShmGetImage(d,src,img,0,0,AllPlanes);
    else { XImage*g=XGetImage(d,src,0,0,W,H,AllPlanes,ZPixmap); if(g){memcpy(img->data,g->data,(size_t)img->bytes_per_line*H); XDestroyImage(g);} else memset(img->data,0,(size_t)img->bytes_per_line*H); }
    unsigned char*p=(unsigned char*)img->data; int bpl=img->bytes_per_line;
    double sr=0,sg=0,sb=0,sd=0;
    for(int y=0;y<hh;y++){unsigned char*a=p+(ds*y)*bpl,*b=a+bpl,*o=half+y*hw*4,*pv=prev+y*hw;
      for(int x=0;x<hw;x++){int B=(a[0]+a[4]+b[0]+b[4]+2)>>2,G=(a[1]+a[5]+b[1]+b[5]+2)>>2,R=(a[2]+a[6]+b[2]+b[6]+2)>>2;o[0]=B;o[1]=G;o[2]=R;o[3]=0;
        sb+=B;sg+=G;sr+=R;int l=(R+2*G+B)>>2;int df=l-pv[x];sd+=df<0?-df:df;pv[x]=l;a+=4*ds;b+=4*ds;o+=4;}}
    double n=hw*(double)hh; fprintf(fc,"%.4f,%.3f,%.3f,%.3f,%.3f\n",t-tl,sr/n,sg/n,sb/n,sd/n);
    fwrite(half,1,hw*hh*4,fv);
    if(shot_i<nshots && t-tl>=shots[shot_i]){ snprintf(fn,sizeof fn,"%s.shot_%06.2f.ppm",prefix,shots[shot_i]); FILE*fs=fopen(fn,"wb"); fprintf(fs,"P6\n%d %d\n255\n",W,H);
      unsigned char*row=malloc(W*3); for(int y=0;y<H;y++){unsigned char*a=p+y*bpl;for(int x=0;x<W;x++){row[3*x]=a[4*x+2];row[3*x+1]=a[4*x+1];row[3*x+2]=a[4*x];}fwrite(row,1,W*3,fs);} free(row); fclose(fs); shot_i++; }
    k++;
    if(pid&&texit<0){int st; if(waitpid(pid,&st,WNOHANG)==pid){texit=now()-T0; status=st; fprintf(fi,"exit_time %.6f\nrun_seconds %.3f\nstatus %d exitcode %d signal %d\n",texit,texit-tl,st,WIFEXITED(st)?WEXITSTATUS(st):-1,WIFSIGNALED(st)?WTERMSIG(st):0); fflush(fi);
        if(dur>texit+1.5) dur=texit+1.5; }}
  }
  if(pid&&texit<0){kill(pid,SIGKILL);fprintf(fi,"killed_at_end 1\n");}
  fprintf(fi,"frames %ld\n",k); fclose(fi); fclose(fc); pclose(fv);
  stop_all=1; pthread_join(th,0);
  return 0;
}
