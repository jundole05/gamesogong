#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace tutorial {
// Procedural material coordinates are anchored in the world (terrain) or each face
// (buildings), so the grain and ripples do not slide when the camera moves.
static const char* MaterialFragment = R"GLSL(#version 330 core
in vec2 texcoord; in vec4 tint; flat in float surface;
uniform sampler2D tex; uniform float time; out vec4 result;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p); f=f*f*(3.-2.*f);
return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
void main(){
 if(surface<.5){result=texture(tex,texcoord)*tint;return;}
 vec2 p=texcoord; vec3 c=tint.rgb; float n=noise(p*21.);
 if(surface<1.5){ // grass: fine blades and broad patches
   c*=.85+.21*noise(p*1.8)+.13*n;
   c+=vec3(.015,.035,.008)*pow(noise(p*vec2(80,13)),7.);
 }else if(surface<2.5){ // irregular cobbles with recessed mortar
   vec2 q=p*3.2; q.x+=mod(floor(q.y),2.)*.5;
   vec2 f=fract(q); float edge=min(min(f.x,1.-f.x),min(f.y,1.-f.y));
   c*=mix(.65,.88+.23*hash(floor(q)),smoothstep(.025,.12,edge)); c+=n*.035;
 }else if(surface<3.5){ // lake: intersecting waves, moving caustics and glints
   float wave=sin(p.x*6.+p.y*4.+time*1.1)+sin(p.x*3.-p.y*7.-time*.8);
   float caustic=pow(.5+.5*sin(p.x*15.+p.y*11.+wave+time),16.);
   c*=.82+.13*wave; c+=vec3(.12,.30,.28)*caustic;
   c+=vec3(.7,.8,.65)*pow(max(0.,sin(p.x*21.+wave)*sin(p.y*17.-time*.7)),38.);
 }else if(surface<4.5){ // stucco / stone wall
   vec2 q=p*vec2(7,6); q.x+=mod(floor(q.y),2.)*.5;
   vec2 f=fract(q); float seam=smoothstep(.015,.06,min(f.x,f.y));
   c*=.78+.17*n+.14*seam;
 }else if(surface<5.5){ // overlapping roof shingles
   vec2 q=p*vec2(12,8); q.x+=mod(floor(q.y),2.)*.5;
   vec2 f=fract(q); float seam=smoothstep(.02,.10,min(f.x,f.y));
   c*=.62+.30*seam+.17*f.y+.12*hash(floor(q));
 }else{ // timber grain and plank seams
   float grain=noise(p*vec2(9,110));
   c*=.73+.23*grain+.12*sin(p.y*200.+noise(p*8.)*7.);
   c*=mix(.65,1.,smoothstep(.015,.06,fract(p.x*5.)));
 }
 result=vec4(c,tint.a);
})GLSL";

static const char* PostVertex = R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0,1);}
)GLSL";
static const char* PostFragment = R"GLSL(#version 330 core
in vec2 uv; out vec4 result; uniform sampler2D source; uniform sampler2D bloom;
uniform int mode; uniform vec2 stepSize;
void main(){
 vec3 c=texture(source,uv).rgb;
 if(mode==0){float l=max(c.r,max(c.g,c.b));result=vec4(c*smoothstep(.62,1.05,l),1);return;}
 if(mode==1){
   c*=.227027;
   c+=(texture(source,uv+stepSize*1.384615).rgb+texture(source,uv-stepSize*1.384615).rgb)*.316216;
   c+=(texture(source,uv+stepSize*3.230769).rgb+texture(source,uv-stepSize*3.230769).rgb)*.070270;
   result=vec4(c,1);return;
 }
 // Edge-sensitive five-tap smoothing; HUD is composited afterwards.
 vec3 n=texture(source,uv+vec2(0,stepSize.y)).rgb,s=texture(source,uv-vec2(0,stepSize.y)).rgb;
 vec3 e=texture(source,uv+vec2(stepSize.x,0)).rgb,w=texture(source,uv-vec2(stepSize.x,0)).rgb;
 float edge=length(n-s)+length(e-w); c=mix(c,(c*4.+n+s+e+w)/8.,smoothstep(.18,.65,edge)*.6);
 c+=texture(bloom,uv).rgb*.23;
 c=mix(vec3(dot(c,vec3(.2126,.7152,.0722))),c,1.06);
 c=c*vec3(1.035,1.015,.975); c=(c-.5)*1.045+.5;
 float vignette=1.-.17*smoothstep(.15,.76,length((uv-.5)*vec2(1.1,1.)));
 result=vec4(clamp(c*vignette,0.,1.),1);
})GLSL";

// Runtime-generated RGBA sprite sheet: 8 walking frames x 8 facing directions.
// Each cell is 64x80. No external image or font-generation build step is needed.
inline std::vector<unsigned char> MakeCharacterSheet(unsigned costume) {
    const int width=512,height=640;
    std::vector<unsigned char> pixels(width*height*4,0);
    for(int dir=0;dir<8;++dir) for(int frame=0;frame<8;++frame) {
        float angle=dir*6.2831853f/8, turn=std::cos(angle), back=std::sin(angle);
        float gait=std::sin(frame*6.2831853f/8), bob=std::abs(gait)*1.3f;
        auto ellipse=[&](float cx,float cy,float rx,float ry,unsigned color) {
            for(int y=(std::max)(0,int(cy-ry-1));y<(std::min)(80,int(cy+ry+2));++y)
            for(int x=(std::max)(0,int(cx-rx-1));x<(std::min)(64,int(cx+rx+2));++x) {
                float dx=(x+.5f-cx)/rx,dy=(y+.5f-cy)/ry;
                float a=(std::min)(1.f,(std::max)(0.f,(1.f-dx*dx-dy*dy)*5.f));
                if(a<=0)continue;
                size_t i=((dir*80+y)*width+frame*64+x)*4;
                float old=pixels[i+3]/255.f, out=a+old*(1-a);
                for(int k=0;k<3;++k){float v=float((color>>(16-k*8))&255);
                    pixels[i+k]=static_cast<unsigned char>((v*a+pixels[i+k]*old*(1-a))/out);}
                pixels[i+3]=static_cast<unsigned char>(out*255);
            }
        };
        // Layered boots, articulated legs and arms, tunic, leather and face.
        ellipse(26,65+gait*4,5,7,0x24313a); ellipse(38,65-gait*4,5,7,0x24313a);
        ellipse(26,57+gait*2,4,9,0x48515b); ellipse(38,57-gait*2,4,9,0x48515b);
        ellipse(21,43-gait*3,4,11,0x29383e); ellipse(43,43+gait*3,4,11,0x29383e);
        ellipse(20,50-gait*3,3,4,0xdcb68f); ellipse(44,50+gait*3,3,4,0xdcb68f);
        ellipse(32,44-bob,13,17,0x29383e); ellipse(32,43-bob,11,16,costume);
        ellipse(28,41-bob,3,12,0x96b4ab); ellipse(32,53-bob,11,2,0x614b36);
        ellipse(33,53-bob,2,2,0xddbd72);
        ellipse(23,33-bob,5,4,0xb5babb); ellipse(41,33-bob,5,4,0x7c8f91);
        ellipse(32,28-bob,4,5,0xbb916e); ellipse(32,20-bob,10,12,0x353335);
        ellipse(32+turn*2,22-bob,8,9,0xe0ba94);
        ellipse(30,13-bob,10,6,0x534637); ellipse(26,19-bob,4,7,0x534637);
        if(back<-.3f) ellipse(32,21-bob,9,10,0x534637);
        else { ellipse(31+turn*4,22-bob,1.1f,1.4f,0x303844); ellipse(35+turn*4,22-bob,1.1f,1.4f,0x303844); }
        ellipse(31,31-bob,8,2.5f,0xcfb17a);
        if(back<0) {ellipse(32,42-bob,9,14,costume);ellipse(29,44-bob,2,11,0x96b4ab);}
    }
    return pixels;
}
}
