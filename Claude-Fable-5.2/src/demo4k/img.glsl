uniform float t;
float B,o,zf,e,k,T,K;      // beats, global order, crystal face plane, emissive, local order, text scene, kick
vec3 cp;
float N(vec3 p){
  vec3 i=floor(p),f=p-i;f=f*f*(3.-2.*f);
  float n=i.x+i.y*57.+i.z*113.;
  return mix(mix(mix(H(n),H(n+1.),f.x),mix(H(n+57.),H(n+58.),f.x),f.y),
             mix(mix(H(n+113.),H(n+114.),f.x),mix(H(n+170.),H(n+171.),f.x),f.y),f.z);
}
// one mote per unit cell: wandering while loose, locked to the lattice site when ordered
float D(vec3 p){
  vec3 c=floor(p),q=p-c-.5;
  float h=dot(c,vec3(1,57,113)),b=0.;
  ivec2 u=ivec2(c.x+15.,2.-c.y);
  // M O R P H as 5x5 dot glyphs on the front layer of the crystal, lit column by column on 16th notes
  if(c.z==zf&&u.x>=0&&u.x<30&&u.y>=0&&u.y<5&&u.x%6<5)
    b=float(int[5](18405233,15255086,18136623,1097263,18415153)[u.x/6]>>(u.x%6+u.y*5)&1)*step(float(u.x),B*4.-400.);
  k=smoothstep(0.,.3,o*1.3-N(c*.21));
  // motes appear one by one; every site is occupied once the cell is ordered
  float v=step(H(h+.5),t*t*.002+k);
  e=max(max(b*(1.+K),v*(1.-k)*(.2+.8*H(h+floor(t*9.)))*(.5+K)),             // text / flickering loose motes
        k*step(.97,H(h+floor(B*4.)*.37))*exp(-fract(B*4.)*3.));         // arpeggio sparks
  float l=length(q-sin(vec3(H(h),H(h+.1),H(h+.2))*6.28+t*(.6+H(h+.3)))*.3*(1.-k))-.04-.11*k-.13*b+1.-v;
  l=min(l,min(length(q.xy),min(length(q.yz),length(q.zx)))+.97-smoothstep(.6,1.,k));  // bonds form last
  return max(min(l,.3),zf-p.z);
}
void main(){
  B=t*32./15.;
  o=smoothstep(32.,64.,B);
  K=exp(-fract(B)*4.)*step(32.,B);
  T=step(96.,B);
  float z=70.*smoothstep(0.,1.,pow(min(t/45.,1.),1.7))-19.*T+(t-45.)*T*.1,
        a=.5*sin(t*.17)*(1.-T)+.05*T,s=0.,d,g=0.,f=0.,i=0.;
  cp=vec3(-.5*T,-.5*T,z);
  zf=mix(-99.,70.,T);
  vec2 uv=(gl_FragCoord.xy-vec2(960,540))/540.;uv.y=-uv.y;
  vec3 rd=normalize(vec3(uv*mat2(cos(a),sin(a),-sin(a),cos(a)),1.6-.12*K)),p;
  rd.xy+=.2*vec2(sin(t*.31),cos(t*.23))*(1.-T);
  a=.6*sin(t*.4)*o*(1.-T);
  rd.xz*=mat2(cos(a),sin(a),-sin(a),cos(a));
  for(;i<220.;i++){
    p=cp+rd*s;d=D(p);
    g+=e*d*exp(-d*30.);
    f+=(1.-k)*max(0.,N(p*.5+t*.06)+.5*N(p*1.3-t*.1)-.8)*d;
    if(d<.001*s||s>70.)break;
    s+=d*.8;
  }
  vec3 A=mix(vec3(.45,.65,1.),vec3(1.,.5,.12),o),F=vec3(.01,.02,.04)*(1.+K)+A*.3*o*(1.-T)*exp(-2.*dot(uv,uv)),col=F,L=normalize(vec3(.5,.7,-.6));
  if(d<.001*s){
    float kk=k,ee=e,w=exp(-.7*abs(p.z-cp.z-2.-28.*fract(B)))*step(32.,B)*(1.-T)*k;      // kick: a sheet of light runs down the lattice
    vec2 h=vec2(.002,0);
    vec3 n=normalize(vec3(D(p+h.xyy),D(p+h.yxy),D(p+h.yyx))-d);
    col=mix(F,(vec3(.45,.52,.64)*(1.+K)+A*w*9.)*(max(0.,dot(n,L))*.7+.1)*(1.-i/220.)
       +pow(1.+dot(rd,n),3.)*(vec3(.2,.4,.8)+A*(g*40.+w*3.))+pow(max(0.,dot(reflect(rd,n),L)),24.)*.7,exp(-s*(.05+.1*T)))
       +A*ee*4.;
  }
  col+=A*g*40.;
  col=mix(col,vec3(.16,.2,.28),(1.-exp(-f*1.2))*smoothstep(3.,14.,t));
  col+=A*(exp(-max(B-96.,0.)*1.5)*T+exp(-max(B-112.,0.)*3.)*step(112.,B)*.5);        // flashes on the cut and the final chord
  col=1.-exp(-col*1.6);
  col=col*col*(3.-2.*col);
  // static: the picture condenses out of noise
  col=mix(col,vec3(H(dot(gl_FragCoord.xy,vec2(.37,.91))+t*7.))*.7,.95*exp(-t*.6)+.03);
  col*=(1.-.15*dot(uv,uv))*smoothstep(60.,57.5,t)*smoothstep(0.,.5,t);
  gl_FragColor=vec4(pow(max(col,0.),vec3(.4545)),1);
}
