#version 330 core
uniform vec2 R;
uniform float T;
out vec4 O;
float h(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float w(vec2 p){
    if(p.x<0.||p.x>5.8||p.y<0.||p.y>1.)return 0.;
    int i=int(p.x/1.2),c=655257;
    if(i==1)c=432534;if(i==2)c=958121;if(i==3)c=958088;if(i==4)c=630681;
    p.x=mod(p.x,1.2);if(p.x>1.)return 0.;
    ivec2 j=ivec2(p*vec2(4,5));
    return float((c>>(j.y*4+3-j.x))&1);
}
void main(){
    vec2 u=(2.*gl_FragCoord.xy-R)/R.y;
    float l=length(u),a=atan(u.y,u.x);
    float o=smoothstep(6.,28.,T),z=smoothstep(32.,51.,T),c=1.-o;
    float E=exp(-8.*fract(T*(32./15.)));
    float n=h(floor(gl_FragCoord.xy*.5)+floor(T*16.));
    float f=sin(l*(18.+8.*o)-T*3.+sin(a*(4.+4.*o)+T)*c*4.+n*c*2.);
    float lines=exp(-11.*abs(f))*smoothstep(1.45,.12,l);
    lines*=.55+.45*exp(-16.*abs(sin(a*8.+l*5.-T)));
    vec3 col=vec3(.002,.006,.012);
    col+=lines*mix(vec3(.02,.55,.32),vec3(.03,.28,.8),o)*(.15+.35*o);
    float ring=exp(-80.*abs(l-.46-.1*cos(a*6.+T*.2)));
    col+=ring*z*vec3(.1,.8,.55)*(.5+.25*E);
    col+=.05*E*vec3(.02,.25,.18)/(.1+dot(u,u));
    float title=smoothstep(47.,50.,T);
    vec2 q=u/vec2(.16,.14)+vec2(2.9,.5);
    col+=w(q)*title*vec3(.25,1.,.65)*(1.1+.25*E);
    col+=(n-.5)*mix(.1,.012,o);
    col=pow(max(1.-exp(-col*1.4),0.),vec3(.4545));
    col*=smoothstep(0.,1.,T)*(1.-smoothstep(59.,60.,T));
    O=vec4(col,1.);
}
