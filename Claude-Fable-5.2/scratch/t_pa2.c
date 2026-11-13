#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <pulse/simple.h>
#include <pulse/error.h>
static double now(){struct timespec t;clock_gettime(CLOCK_REALTIME,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char**argv){
  int tl_ms=argc>1?atoi(argv[1]):100;
  pa_sample_spec ss={PA_SAMPLE_FLOAT32LE,48000,2}; int err=0;
  pa_buffer_attr ba={(unsigned)-1,(unsigned)(48000*8*tl_ms/1000),(unsigned)-1,(unsigned)-1,(unsigned)-1};
  pa_simple*s=pa_simple_new(0,"morph",PA_STREAM_PLAYBACK,0,"morph",&ss,0,tl_ms>0?&ba:0,&err);
  if(!s){printf("pa_simple_new failed: %s\n",pa_strerror(err));return 1;}
  int N=48000*4; float*b=malloc(N*2*sizeof(float));
  for(int i=0;i<N;i++){float v=0.3f*sinf(i*2*M_PI*440/48000)*(i<48000*3); b[2*i]=v;b[2*i+1]=v;}
  double t1=now(); printf("START_WALL %.6f\n",t1);
  int chunk=48000/50*8; // 20ms in bytes
  for(int off=0,k=0; off<N*8; off+=chunk,k++){ pa_simple_write(s,(char*)b+off,chunk,&err); if(k%10==0&&k<120) {double w=now()-t1; double lat=pa_simple_get_latency(s,&err)/1e6; double at=(off+chunk)/384000.0; printf("  audio written %.2fs at wall %.3fs  latency=%.0f ms  => playpos-wall = %.1f ms\n",at,w,lat*1e3,(at-lat-w)*1e3);} }
  printf("all written at %.3f s\n",now()-t1);
  pa_simple_drain(s,&err); printf("drained at %.3f s\n",now()-t1);
  pa_simple_free(s); return 0;
}
