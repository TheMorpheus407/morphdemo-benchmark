#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <pulse/simple.h>
#include <pulse/error.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){
  pa_sample_spec ss={PA_SAMPLE_FLOAT32LE,48000,2}; int err=0;
  double t0=now();
  pa_simple*s=pa_simple_new(0,"morph",PA_STREAM_PLAYBACK,0,"morph",&ss,0,0,&err);
  if(!s){printf("pa_simple_new failed: %s\n",pa_strerror(err));return 1;}
  printf("connect took %.1f ms\n",(now()-t0)*1e3);
  int N=48000*4; float*b=malloc(N*2*sizeof(float));
  for(int i=0;i<N;i++){float v=0.3f*sinf(i*2*M_PI*440/48000)*(i<48000*3); b[2*i]=v;b[2*i+1]=v;}
  double t1=now();
  int chunk=48000/10*8; // 100ms in bytes
  for(int off=0; off<N*8; off+=chunk){ pa_simple_write(s,(char*)b+off,chunk,&err); if(off<chunk*40 && (off/chunk)%5==0) printf("  wrote %.1fs of audio at wall %.3fs  latency=%.1f ms\n",(off+chunk)/384000.0,now()-t1,pa_simple_get_latency(s,&err)/1e3); }
  printf("all written at %.3f s\n",now()-t1);
  pa_simple_drain(s,&err); printf("drained at %.3f s\n",now()-t1);
  pa_simple_free(s); return 0;
}
