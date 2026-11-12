/* demo16k.c — compact FROM NOISE TO ORDER, built to fit 16,384 bytes. */
typedef unsigned char      Uint8;
typedef unsigned short     Uint16;
typedef unsigned int       Uint32;
typedef unsigned long long Uint64;
typedef void              *SDL_GLContext;
typedef struct SDL_Window  SDL_Window;
typedef void             (*SDL_AudioCallback)(void *, Uint8 *, int);
typedef struct {
    int freq; Uint16 format; Uint8 channels; Uint8 silence; Uint16 samples;
    Uint16 padding; Uint32 size; SDL_AudioCallback callback; void* userdata;
} SDL_AudioSpec;
#define SDL_INIT_VIDEO 0x20u
#define SDL_INIT_AUDIO 0x10u
#define SDL_WINDOW_OPENGL 0x2u
#define SDL_AUDIO_F32 0x8010u
#define SDL_SCANCODE_ESCAPE 41
extern int SDL_Init(Uint32); extern SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,Uint32);
extern SDL_GLContext SDL_GL_CreateContext(SDL_Window*); extern int SDL_GL_MakeCurrent(SDL_Window*,SDL_GLContext);
extern int SDL_GL_SetSwapInterval(int); extern void SDL_GL_SwapWindow(SDL_Window*);
extern void SDL_PumpEvents(void); extern const Uint8* SDL_GetKeyboardState(int*);
extern int SDL_OpenAudio(SDL_AudioSpec*,SDL_AudioSpec*); extern void SDL_PauseAudio(int);
extern void SDL_Quit(void); extern Uint64 SDL_GetPerformanceCounter(void); extern Uint64 SDL_GetPerformanceFrequency(void);
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint; typedef int GLsizei;
typedef char GLchar; typedef float GLfloat; typedef unsigned int GLbitfield; typedef unsigned char GLboolean;
typedef long GLsizeiptr; typedef long GLintptr;
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_POINTS 0
#define GL_TRIANGLES 4
#define GL_COLOR_BUFFER_BIT 0x4000u
#define GL_BLEND 0x0BE2
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_FLOAT 0x1406
#define GL_FALSE 0
extern void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat); extern void glClear(GLbitfield);
extern void glViewport(GLint,GLint,GLsizei,GLsizei); extern void glUseProgram(GLuint);
extern GLuint glCreateProgram(void); extern void glAttachShader(GLuint,GLuint);
extern void glLinkProgram(GLuint); extern void glDeleteShader(GLuint);
extern GLuint glCreateShader(GLenum); extern void glShaderSource(GLuint,GLsizei,const GLchar*const*,const GLint*);
extern void glCompileShader(GLuint); extern GLint glGetUniformLocation(GLuint,const GLchar*);
extern void glUniform1f(GLint,GLfloat); extern void glUniform2f(GLint,GLfloat,GLfloat);
extern void glUniform4f(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
extern void glGenBuffers(GLsizei,GLuint*); extern void glBindBuffer(GLenum,GLuint);
extern void glBufferData(GLenum,long,const void*,GLenum);
extern void glBufferSubData(GLenum,long,long,const void*);
extern void glGenVertexArrays(GLsizei,GLuint*); extern void glBindVertexArray(GLuint);
extern void glEnableVertexAttribArray(GLuint); extern void glVertexAttribPointer(GLuint,GLint,GLenum,unsigned char,GLsizei,const void*);
extern void glDrawArrays(GLenum,GLint,GLsizei); extern void glEnable(GLenum);
extern void glBlendFunc(GLenum,GLenum);
#define NP 512
typedef struct { float x,y,z; } V;
static V g_pos[NP],g_col[NP],g_home[NP],g_hcol[NP]; static unsigned int g_seed[NP];
static unsigned int g_rng=0x243F6A88u;
static float rn(void){ g_rng=g_rng*1103515245u+12345u; return (float)(g_rng>>16)/32768.0f-1.0f; }
static float r01(void){ return rn()*0.5f+0.5f; }
static float ms(float x){ while(x>3.14159265f)x-=6.28318530f; while(x<-3.14159265f)x+=6.28318530f; float x2=x*x; return x*(1-x2/6+x2*x2/120-x2*x2*x2/5040); }
static V v3(float x,float y,float z){ V v; v.x=x;v.y=y;v.z=z; return v; }
static void pi(void){ for(int i=0;i<NP;i++){ g_seed[i]=i*2654435761u+0x9E3779B9u; float fi=(float)i/NP; int ty=i%3; V h,c;
 if(ty==0){ float a=fi*6.2831853f,r=ms(a*3)*.5f+.5f; h=v3(ms(a)*r*5,rn()*5,ms(a*2)*r*5); c=v3(.6f,.4f,.9f); }
 else if(ty==1){ float a=fi*6.2831853f*2; h=v3(ms(a)*4,(fi*2-1)*4,ms(a*1.3f)*4); c=v3(.2f,.8f,.6f); }
 else { h=v3((fi*4-2)*2.2f,rn()*1.5f,rn()*1.5f); c=v3(.9f,.7f,.3f); }
 g_home[i]=h; g_hcol[i]=c; g_pos[i]=v3(rn()*6,rn()*6,rn()*6); g_col[i]=v3(r01(),r01(),r01()); } }
static V nv(int i,double t){ unsigned int s=g_seed[i]+(unsigned int)(t*1000)*1597334677u; s=s*1103515245u+12345u; float x=(float)(s>>16)/32768.0f-1;
 s=s*1103515245u+12345u; float y=(float)(s>>16)/32768.0f-1; s=s*1103515245u+12345u; float z=(float)(s>>16)/32768.0f-1; return v3(x,y,z); }
static volatile double g_time; static double g_at=0;
typedef struct { float pha,freq,amp,tgt; int mode; } W; static W gv[1];
static float ot(W*v){ v->amp+=(v->tgt-v->amp)*0.1f; if(v->amp<0.0005f)v->amp=0; v->pha+=v->freq; while(v->pha>=1)v->pha-=1; while(v->pha<0)v->pha+=1;
 float p=v->pha*2-1,ap=p<0?-p:p,osc; switch(v->mode){case 0:osc=p;break;case 1:osc=p<0?-1:1;break;default:osc=1-2*ap;break;} return osc*v->amp; }
static float nfreq(int);
static float nfreq(int);
static void am(void*u,Uint8*s,int l){ int nf=l/8; float*o=(float*)s; double t=g_at;
 for(int i=0;i<nf;i++,t+=1.0/48000.0){ double bt=t*2; long long bi=(long long)bt; double f=bt-bi; float pulse=(f<0.03f)?1-f/0.03f:0; float l=0,r=0; int ph=(t<15)?0:(t<40)?1:2;
  if(ph==0){ float nz=rn()*.35f; long long b=(long long)bt; float bs=((b%4==0)&&f<0.04f)?.5f:0; l=nz+bs; r=nz*.7f+bs*.6f; }
  else if(ph==1){ long long b=(long long)bt; int no=57+(int)(b%8); gv[0].mode=1; gv[0].freq=(float)(nfreq(no)/48000); gv[0].tgt=(f<.75f)?.35f:0; float s=ot(&gv[0]); l+=s*.5f;r+=s*.46f;
   if(b%4==0&&f<.25f){ gv[0].mode=0;gv[0].freq=(float)(nfreq(36)/48000);gv[0].tgt=.7f;float sb=ot(&gv[0]);l+=sb*.6f;r+=sb*.6f;} }
  else { gv[0].mode=2;gv[0].freq=(float)(nfreq(60)/48000);gv[0].tgt=.45f;float s=ot(&gv[0]);l+=s*.5f;r+=s*.46f; if(t>58){float m=(float)((60-t)/2);l*=m;r*=m;} }
  float m=(ph==0)?.3f:(ph==1)?.55f:.85f; l=l*m+pulse*.2f;r=r*m+pulse*.2f; if(l>1)l=1;if(l<-1)l=-1;if(r>1)r=1;if(r<-1)r=-1; o[2*i]=l*.5f;o[2*i+1]=r*.5f; } g_at=t; }
static float ss(float t){ if(t<=0)return 0; if(t>=1)return 1; return t*t*(3-2*t); }
static float nfreq(int no){
 static const float s[12]={1,1.059463f,1.122462f,1.189207f,1.259921f,1.334840f,1.414214f,1.498307f,1.587401f,1.681793f,1.781797f,1.887749f};
 int rel=no-69,oct=rel>=0?rel/12:-((-rel+11)/12),sem=rel-oct*12; float f=440*s[sem];
 while(oct>0){f*=2;oct--;} while(oct<0){f*=0.5f;oct++;} return f; }
static const unsigned char font[5][5]={{0b10001,0b11011,0b10101,0b10001,0b10001},{0b01110,0b10001,0b10001,0b10001,0b01110},{0b01111,0b10001,0b01111,0b00101,0b10001},{0b01111,0b10001,0b01111,0b00001,0b00001},{0b10001,0b10001,0b01110,0b10001,0b10001}};
static int pof(double t){ return t<15?0:(t<40?1:2); }
static const char*vs="\
#version 330 core\nlayout(location=0) in vec3 p;\nlayout(location=1) in vec3 c;\nlayout(location=0) uniform float a;layout(location=1) uniform float d;layout(location=2) uniform float f;layout(location=3) uniform vec2 r;out vec3 v;\n\
void main(){float s=sin(a),co=cos(a);float x=p.x*co+p.z*s,z=-p.x*s+p.z*c+d;gl_Position=vec4(vec2(x,p.y)*(f/z)*vec2(2.0/r.x,2.0/r.y),0,1);gl_PointSize=2.0+8.0/z;v=c;}\n";
static const char*fs="\
#version 330 core\nin vec3 v;out vec4 f;\nvoid main(){float d=length(gl_PointCoord-vec2(0.5));float a=1.0-d*2.0;if(a<=0)discard;f=vec4(v,a*a*0.9);}\n";
static const char*vst="\
#version 330 core\nlayout(location=0) in vec2 p;\nlayout(location=0) uniform vec2 r;void main(){gl_Position=vec4(p*2.0/r-vec2(1.0),0,1);}\n";
static const char*fst="\
#version 330 core\nlayout(location=0) uniform vec4 c;out vec4 f;\nvoid main(){f=c;}\n";
static GLuint sh(GLenum ty,const char*src){ GLuint s=glCreateShader(ty); glShaderSource(s,1,&src,0); glCompileShader(s); return s; }
static GLuint pg(const char*a,const char*b){ GLuint p=glCreateProgram(); GLuint vs=sh(GL_VERTEX_SHADER,a),fs=sh(GL_FRAGMENT_SHADER,b); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p); glDeleteShader(vs); glDeleteShader(fs); return p; }
static float tv[64*6*2]; static int tc=0;
static void tq(float x,float y,float w,float h){ if(tc+6>384)return; float*v=&tv[tc*2]; v[0]=x;v[1]=y;tc++; v[2]=x+w;v[3]=y;tc++; v[4]=x+w;v[5]=y+h;tc++; v[6]=x;v[7]=y;tc++; v[8]=x+w;v[9]=y+h;tc++; v[10]=x;v[11]=y+h;tc++; }
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO))return 1;
 SDL_Window*w=SDL_CreateWindow("morphdemo",0,0,1920,1080,SDL_WINDOW_OPENGL); if(!w)return 2;
 SDL_GLContext gc=SDL_GL_CreateContext(w); SDL_GL_MakeCurrent(w,gc); SDL_GL_SetSwapInterval(1);
 SDL_AudioSpec as; as.freq=48000;as.format=0x8010u;as.channels=2;as.silence=0;as.samples=1024;as.size=0;as.callback=am;as.userdata=0;
 SDL_OpenAudio(&as,&as); SDL_PauseAudio(0); glClearColor(0,0,0,1);
 GLuint pp=pg(vs,fs),pt=pg(vst,fst);
 GLint pa0=glGetUniformLocation(pp,"a"),pa1=glGetUniformLocation(pp,"d"),pa2=glGetUniformLocation(pp,"f"),pa3=glGetUniformLocation(pp,"r");
 GLint pt0=glGetUniformLocation(pt,"r"),pt1=glGetUniformLocation(pt,"c");
 GLuint vao,vbo; glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
 glBufferData(GL_ARRAY_BUFFER,sizeof(g_pos)+sizeof(g_col),0,GL_DYNAMIC_DRAW);
 glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(const void*)0);
 glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(const void*)sizeof(g_pos));
 glEnable(GL_PROGRAM_POINT_SIZE);
 GLuint tvao,tvbo; glGenVertexArrays(1,&tvao); glGenBuffers(1,&tvbo); glBindVertexArray(tvao); glBindBuffer(GL_ARRAY_BUFFER,tvbo);
 glBufferData(GL_ARRAY_BUFFER,sizeof(tv),0,GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);
 pi();
 Uint64 fq=SDL_GetPerformanceFrequency(),t0=SDL_GetPerformanceCounter();
 while(1){ Uint64 nn=SDL_GetPerformanceCounter(); double t=(double)(nn-t0)/(double)fq; if(t>=60)break;
  SDL_PumpEvents(); const Uint8*ks=SDL_GetKeyboardState(0); if(ks&&ks[41])break;
  g_time=t; int ph=pof(t); float seg=(ph==0)?t/15:(ph==1)?(t-15)/25:(t-40)/20; float sm=ss(seg); if(seg>1)sm=1; float m,nn2;
  if(ph==0){m=0;nn2=1;}else if(ph==1){m=sm;nn2=1-sm;}else{m=1;nn2=0;}
  double bt=t*2; long long bi=(long long)bt; double f=bt-bi; float pulse=(f<0.03f)?1-f/0.03f:0; nn2+=pulse*.4f;
  float ang=(float)(t*.25f); float cd=(ph==0)?12:(ph==1)?10-m*3:7.5f;
  for(int i=0;i<NP;i++){ V r=nv(i,t); V ta=v3(g_home[i].x*m+r.x*nn2,g_home[i].y*m+r.y*nn2,g_home[i].z*m+r.z*nn2);
   g_pos[i]=v3(g_pos[i].x+(ta.x-g_pos[i].x)*.2f,g_pos[i].y+(ta.y-g_pos[i].y)*.2f,g_pos[i].z+(ta.z-g_pos[i].z)*.2f);
   V rc=v3(r01(),r01(),r01()); V c=v3(rc.x+(g_hcol[i].x-rc.x)*m,rc.y+(g_hcol[i].y-rc.y)*m,rc.z+(g_hcol[i].z-rc.z)*m); float lu=.5f+m*.5f+pulse*.5f;
   g_col[i]=v3(c.x*lu,c.y*lu,c.z*lu);}
  glViewport(0,0,1920,1080); glClear(GL_COLOR_BUFFER_BIT);
  glBindBuffer(GL_ARRAY_BUFFER,vbo); glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(g_pos),g_pos); glBufferSubData(GL_ARRAY_BUFFER,sizeof(g_pos),sizeof(g_col),g_col);
  glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE); glUseProgram(pp);
  glUniform1f(pa0,ang); glUniform1f(pa1,cd); glUniform1f(pa2,935); glUniform2f(pa3,1920,1080);
  glBindVertexArray(vao); glDrawArrays(GL_POINTS,0,NP);
  if(ph==2){ tc=0; float tf=ss((float)(t-40)/20); float sc=1+tf*3; float al=tf; float cx=960,cy=540; float cw=14*sc,ch=20*sc; float x=cx-cw*2.5f,y=cy-ch*2.5f;
   for(int li=0;li<5;li++){ float lx=x+li*cw*1.05f; for(int row=0;row<5;row++){ unsigned char b=font[li][row]; float ry=y+row*ch*.22f;
    for(int co=0;co<5;co++){ if(b&(1u<<co)){ float qw=cw*.18f,qh=ch*.22f; tq(lx+co*qw*1.05f,ry,qw,qh);} } } }
   glBindBuffer(GL_ARRAY_BUFFER,tvbo); glBufferSubData(GL_ARRAY_BUFFER,0,tc*8,tv);
   glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glUseProgram(pt); glUniform2f(pt0,1920,1080); glUniform4f(pt1,.95f,.9f,.4f,al*.95f);
   glBindVertexArray(tvao); glDrawArrays(GL_TRIANGLES,0,tc); }
  SDL_GL_SwapWindow(w); }
 SDL_Quit(); return 0; }