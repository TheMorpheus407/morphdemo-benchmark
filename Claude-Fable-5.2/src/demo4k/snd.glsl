// one pixel = one stereo frame: rg = dry mix, ba = echo send. 128 BPM -> a 16th note is exactly 5625 samples,
// so every note-relative time below is computed with exact integer arithmetic (no phase noise late in the track).
vec2 P(float f,float x,float m){vec2 p=6.2832*f*x*vec2(.997,1.003);return sin(p+m*sin(p));}   // detuned stereo FM pair
void main(){
  float n=floor(gl_FragCoord.x)+4096.*floor(gl_FragCoord.y),
        s=floor(n/5625.),x=(n-s*5625.)/48e3,                                // 16th step, seconds into it
        q=floor(n/22500.),y=(n-q*22500.)/48e3,                              // beat, seconds into it
        r=floor(n/11250.),z=(n-r*11250.)/48e3,                              // 8th, seconds into it
        c=min(floor(n/18e4),14.),w=(n-c*18e4)/48e3,                         // chord (2 bars), seconds into it
        b=n/9e4,                                                           // bars
        o=smoothstep(2.,8.,b),                                             // order
        G=step(8.,b)*step(b,30.),                                          // rhythm section on
        k=exp(-y*9.)*G,                                                    // side-chain
        f=110.*exp2(vec4(0,-4,-9,-2)[int(mod(c,4.))]/12.),g,u;
  vec3 C=vec3[4](vec3(0,3,7),vec3(0,3,8),vec3(-2,3,7),vec3(-2,2,5))[int(mod(c,4.))];
  vec2 N=vec2(H(n-s*5625.)-H(n-s*5625.+1.),H(n-s*5625.+.5)-H(n-s*5625.+1.5)),d,v=vec2(0);
  // kick + bass (8th notes on the chord root, FM brightness decays like a filter)
  d=vec2(sin(6.2832*(52.*y+8.*(1.-exp(-y*28.))))*min(1.,y*400.)*exp(-y*8.)*G*.5
        +sin(6.2832*f*(1.+mod(r,2.))*z+2.5*exp(-z*8.)*sin(6.2832*f*z))*min(1.,z*300.)*exp(-z*4.)*(1.-k)*smoothstep(10.,12.,b)*step(b,30.)*.22);
  // hats (differenced noise = high-passed) on 16ths, accent off-beat; clap on 2 and 4 goes to the echo
  d+=N*exp(-x*(60.-35.*mod(s,2.)))*step(16.,b)*step(b,30.)*.12;
  v+=N*exp(-y*24.)*mod(q,2.)*step(16.,b)*step(b,30.)*.2;
  // arpeggio: chord tones, 3-against-4, random at first then locking to the grid and the scale
  g=220.*exp2((C[int(mod(s,3.))]+12.*mod(floor(s/3.),2.)+12.*step(24.,b)+(1.-o)*24.*(H(s)-.5))/12.);
  u=x-(1.-o)*.1*H(s+.3);
  if(u>0.)v+=P(g,u,3.*exp(-u*14.))*(.6+.4*sin(vec2(0,1.57)+s))*exp(-u*(9.-4.*o))*min(1.,u*400.)*smoothstep(1.,6.,b)*(.4+.6*smoothstep(14.,17.,b))*step(b,30.)*.16;
  // pad: three chord notes (+octave), swelling over each chord
  for(int i=0;i<3;i++)
    v+=(P(220.*exp2(C[i]/12.),w,1.2)+.5*P(440.*exp2(C[i]/12.),w,.8))*sin(3.1416*min(w/3.75,1.))*(1.-.6*k)*smoothstep(6.,10.,b)*.07;
  // lead: one long note per chord (E F E D), stepwise over the progression
  v+=P(440.*exp2(vec4(7,8,7,5)[int(mod(c,4.))]/12.),w,.6+.4*sin(w*5.))*(1.-exp(-w*6.))*exp(-w*.4)*step(16.,b)*.07;
  // noise -> order: a cloud of 24 partials, random frequencies gliding onto the harmonics of A, then fading
  u=clamp(b/6.-.33,0.,1.);
  for(float i=0.;i<24.;i++){
    g=150.*exp2(4.*H(i+.7));
    v+=sin(6.2832*(g*n/48e3+(110.*floor(1.+i/3.)-g)*1.875*(6.*(u*u*u-u*u*u*u*.5)+max(0.,b-8.))+H(i))+vec2(0,i))
       *(1.-smoothstep(7.,10.,b))*smoothstep(0.,2.,b)*.022;
  }
  // white-noise riser into every 8-bar section; fade out
  v+=N*pow(fract(b/8.),6.)*step(b,25.)*.13;
  gl_FragColor=vec4(d,v)*smoothstep(32.,30.,b)*.75;
}
