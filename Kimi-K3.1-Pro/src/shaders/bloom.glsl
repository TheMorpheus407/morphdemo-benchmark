#version 330 core
uniform vec2 R;
uniform float T;
uniform float B;
uniform float E;
out vec4 O;
#define PI 3.14159265359
#define TAU 6.28318530718

mat2 rot(float a){float c=cos(a),s=sin(a);return mat2(c,-s,s,c);}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){
    vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1),f.x),f.y);
}
float fbm(vec2 p){
    float f=.5*noise(p);p=p*2.03+4.7;
    f+=.25*noise(p);p=p*2.01+1.3;
    f+=.125*noise(p);return f;
}
float glyph(vec2 p,int code){
    if(p.x<0.||p.x>1.||p.y<0.||p.y>1.)return 0.;
    ivec2 i=ivec2(floor(p*vec2(5,6)));
    int bit=(code>>(i.y*5+4-i.x))&1;
    vec2 f=abs(fract(p*vec2(5,6))-.5);
    return float(bit)*(1.-smoothstep(.33,.48,max(f.x,f.y)));
}
float word(vec2 p){
    if(p.x<0.||p.x>6.18||p.y<0.||p.y>1.)return 0.;
    int i=int(p.x/1.28),code=599442993;
    if(i==1)code=488162862;if(i==2)code=1025462865;
    if(i==3)code=1025458704;if(i==4)code=589284913;
    return glyph(vec2(mod(p.x,1.28)/1.05,p.y),code);
}

void main(){
    vec2 frag=gl_FragCoord.xy;
    vec2 uv=(2.*frag-R)/R.y;
    float order=smoothstep(7.,29.,T);
    float bloom=smoothstep(29.,49.,T);
    float chaos=1.-order;
    vec2 p=uv;
    p*=rot(.055*T+.12*sin(T*.19));
    float warp=fbm(p*2.1+vec2(T*.07,-T*.05));
    vec2 q=p+chaos*.7*vec2(sin(p.y*7.+T*2.+warp*4.),cos(p.x*6.-T*1.7+warp*3.));
    float r=length(q),a=atan(q.y,q.x);
    float pulse=exp(-5.*fract(B));
    vec3 col=vec3(.012,.006,.018);
    col+=vec3(.07,.012,.045)*fbm(p*3.+T*.025);

    float rays=sin(a*(5.+floor(order*4.))-r*(7.+5.*order)+T*1.1+warp*chaos*7.);
    float ribbon=exp(-18.*abs(rays))*smoothstep(1.35,.18,r)*smoothstep(.03,.28,r);
    float ribbon2=exp(-28.*abs(sin(a*4.+r*10.-T*.7-warp*chaos*5.)))*smoothstep(1.6,.3,r);
    vec3 cyan=vec3(.03,.72,.78),coral=vec3(1.,.16,.065),gold=vec3(1.,.61,.12);
    col+=ribbon*mix(coral,cyan,order)*(.16+.35*order);
    col+=ribbon2*mix(cyan,gold,bloom)*(.08+.28*bloom);

    float petals=.47+.20*cos(a*8.+sin(T*.23)*.6)+.06*cos(a*16.-T*.3);
    float flower=exp(-95.*abs(r-petals));
    float inner=exp(-100.*abs(r-(.19+.07*cos(a*8.+T*.15))));
    col+=flower*mix(cyan,gold,bloom)*bloom*(.38+.15*pulse);
    col+=inner*gold*bloom*.6;
    float rings=exp(-45.*abs(sin(r*18.-T*.7+warp*chaos*4.)));
    col+=rings*smoothstep(1.5,.15,r)*mix(coral,cyan,order)*(.035+.11*order);

    for(int i=0;i<18;i++){
        float fi=float(i);
        float ang=fi*2.39996+T*(.08+.02*chaos);
        float rad=.18+fi*.061;
        rad+=chaos*.32*sin(fi*17.+T*1.4);
        vec2 center=vec2(cos(ang),sin(ang))*rad;
        center+=chaos*.18*vec2(sin(fi*9.+T*2.),cos(fi*7.-T*1.3));
        float d=length(p-center);
        float mote=exp(-180.*d)*( .35+.65*hash(vec2(fi,3.)) );
        col+=mote*mix(coral,gold,fi/17.)*(.18+.5*order);
    }

    vec2 grid=uv*95.;
    vec2 id=floor(grid);
    vec2 gv=fract(grid)-.5;
    float dust=step(.965,hash(id))*exp(-24.*length(gv));
    dust*=.4+.6*sin(hash(id+4.)*30.+T*3.)*.5+.5;
    col+=dust*mix(coral,cyan,hash(id+9.))*chaos*1.8;
    float scan=hash(vec2(floor(frag.y*.5),floor(T*18.)))-.5;
    col+=scan*chaos*.045*vec3(1.,.12,.08);

    float core=exp(-7.*r)*(.025+.055*pulse);
    col+=core*mix(coral,gold,bloom);
    float intro=smoothstep(1.2,2.4,T)*(1.-smoothstep(5.,6.7,T));
    float finale=smoothstep(48.,51.,T)*(1.-smoothstep(58.2,59.7,T));
    float alpha=max(intro,finale);
    vec2 tuv=uv-vec2(0.,mix(-.64,0.,finale));
    vec2 tp=tuv/vec2(.15,.125)+vec2(3.09,.5);
    float shadow=word(tp+vec2(.07,-.08));
    float title=word(tp);
    col*=1.-shadow*alpha*.75;
    col+=title*alpha*mix(cyan,gold,finale)*(1.15+.2*pulse);
    float line=exp(-350.*abs(tuv.y+.16))*smoothstep(.78,.25,abs(uv.x));
    col+=line*alpha*mix(coral,gold,finale)*.4;

    col+=(hash(frag+fract(T)*733.)-.5)*mix(.09,.012,order);
    col*=1.-.28*dot(uv*vec2(.48,.7),uv*vec2(.48,.7));
    col=1.-exp(-col*1.35);
    col=pow(max(col,0.),vec3(.4545));
    col*=smoothstep(0.,1.,T)*(1.-smoothstep(59.,60.,T));
    O=vec4(col,1.);
}
