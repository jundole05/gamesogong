#include "stdafx.h"
#include "TutorialRenderer.h"
#include "TutorialVisuals.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace tutorial {
namespace {
GLuint Compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char message[2048] = {};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        std::cerr << message << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}
bool TutorialRenderer::Initialize() {
    const char* vs = "#version 330 core\n"
        "layout(location=0) in vec2 p; layout(location=1) in vec2 uv;"
        "layout(location=2) in vec4 color; layout(location=3) in float material;"
        "out vec2 texcoord; out vec4 tint; flat out float surface;"
        "void main(){gl_Position=vec4(p.x/640.-1.,1.-p.y/360.,0.,1.);texcoord=uv;tint=color;surface=material;}";
    const char* fs = MaterialFragment;
    GLuint v = Compile(GL_VERTEX_SHADER, vs), f = Compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return false; }
    program = glCreateProgram();
    glAttachShader(program, v); glAttachShader(program, f); glLinkProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char message[2048]={}; glGetProgramInfoLog(program,sizeof(message),nullptr,message);
        std::cerr << message << std::endl; Shutdown(); return false;
    }
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(4 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(8*sizeof(float)));
    glGenTextures(1, &white); glBindTexture(GL_TEXTURE_2D, white);
    unsigned char pixel[] = {255,255,255,255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    vertices.reserve(40000);
    if(!CreatePost()) { Shutdown(); return false; }
    return true;
}
bool TutorialRenderer::CreatePost() {
    GLuint v=Compile(GL_VERTEX_SHADER,PostVertex),f=Compile(GL_FRAGMENT_SHADER,PostFragment);
    if(!v || !f) { if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false; }
    post=glCreateProgram();glAttachShader(post,v);glAttachShader(post,f);glLinkProgram(post);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(post,GL_LINK_STATUS,&ok);
    if(!ok){char message[2048]={};glGetProgramInfoLog(post,sizeof(message),nullptr,message);std::cerr<<message<<std::endl;return false;}
    auto target=[](GLuint& fbo,GLuint& tex,int w,int h){
        glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    };
    bool complete=target(sceneFbo,sceneTexture,1280,720);
    complete=target(bloomFbo[0],bloomTexture[0],640,360)&&complete;
    complete=target(bloomFbo[1],bloomTexture[1],640,360)&&complete;
    glBindFramebuffer(GL_FRAMEBUFFER,0);return complete;
}
void TutorialRenderer::Shutdown() {
    vertices.clear();
    for (auto& entry : labels) glDeleteTextures(1, &entry.second.texture);
    labels.clear(); wrapped.clear();
    for(auto& entry:sprites)glDeleteTextures(1,&entry.second);sprites.clear();
    glDeleteFramebuffers(1,&sceneFbo);glDeleteTextures(1,&sceneTexture);
    glDeleteFramebuffers(2,bloomFbo);glDeleteTextures(2,bloomTexture);
    if(post)glDeleteProgram(post);
    sceneFbo=sceneTexture=post=0;
    bloomFbo[0]=bloomFbo[1]=bloomTexture[0]=bloomTexture[1]=0;
    if (white) glDeleteTextures(1, &white);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (program) glDeleteProgram(program);
    white = vbo = vao = program = active = 0;
}
void TutorialRenderer::Resize(int width, int height) {
    float scale = (std::min)((std::max)(width, 1) / 1280.f, (std::max)(height, 1) / 720.f);
    viewW = (std::max)(1, int(1280 * scale)); viewH = (std::max)(1, int(720 * scale));
    viewX = (width - viewW) / 2; viewY = (height - viewH) / 2;
}
void TutorialRenderer::Begin(float time) {
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glClearColor(.025f, .04f, .06f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER,sceneFbo); glViewport(0,0,1280,720);
    glClear(GL_COLOR_BUFFER_BIT);composited=false;
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program); glUniform1i(glGetUniformLocation(program, "tex"), 0);
    glUniform1f(glGetUniformLocation(program,"time"),time);
    glActiveTexture(GL_TEXTURE0); glBindVertexArray(vao); active = white;
}
void TutorialRenderer::Composite() {
    if(composited)return; Flush();glDisable(GL_BLEND);glUseProgram(post);
    // Never leave a render target bound as the secondary sampled texture.
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,white);
    glUniform1i(glGetUniformLocation(post,"source"),0);glUniform1i(glGetUniformLocation(post,"bloom"),1);
    auto pass=[&](GLuint fbo,GLuint texture,int mode,float dx,float dy){
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
        glUniform1i(glGetUniformLocation(post,"mode"),mode);
        glUniform2f(glGetUniformLocation(post,"stepSize"),dx,dy);glDrawArrays(GL_TRIANGLES,0,3);
    };
    glViewport(0,0,640,360);pass(bloomFbo[0],sceneTexture,0,0,0);
    for(int i=0;i<3;++i){pass(bloomFbo[1],bloomTexture[0],1,1.f/640,0);pass(bloomFbo[0],bloomTexture[1],1,0,1.f/360);}
    glViewport(viewX,viewY,viewW,viewH);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,bloomTexture[0]);
    pass(0,sceneTexture,2,1.f/1280,1.f/720);
    glUseProgram(program);glEnable(GL_BLEND);active=white;composited=true;
}
void TutorialRenderer::End() { if(!composited)Composite();Flush(); }
void TutorialRenderer::Flush() {
    if (vertices.empty()) return;
    glBindTexture(GL_TEXTURE_2D, active); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size())); vertices.clear();
}
void TutorialRenderer::Texture(GLuint texture) { if (active != texture) { Flush(); active = texture; } }
void TutorialRenderer::Push(Vec2 p, Vec2 uv, Color c,float material) { vertices.push_back({p.x,p.y,uv.x,uv.y,c.r,c.g,c.b,c.a,material}); }
void TutorialRenderer::MaterialQuad(Vec2 a,Vec2 b,Vec2 c,Vec2 d,Color color,int material,Vec2 origin,Vec2 span) {
    Texture(white);float m=static_cast<float>(material);
    Vec2 u=origin,v=origin+Vec2(span.x,0),w=origin+span,z=origin+Vec2(0,span.y);
    Push(a,u,color,m);Push(b,v,color,m);Push(c,w,color,m);
    Push(a,u,color,m);Push(c,w,color,m);Push(d,z,color,m);
}
void TutorialRenderer::SoftEllipse(Vec2 p,float rx,float ry,Color color) {
    Texture(white);Color edge=color;edge.a=0;
    for(int i=0;i<40;++i){float a=i*6.2831853f/40,b=(i+1)*6.2831853f/40;
        Push(p,{},color);Push(p+Vec2(std::cos(a)*rx,std::sin(a)*ry),{},edge);Push(p+Vec2(std::cos(b)*rx,std::sin(b)*ry),{},edge);}
}
void TutorialRenderer::Character(Vec2 feet,unsigned color,int direction,int frame,float alpha) {
    auto it=sprites.find(color);
    if(it==sprites.end()) {
        Flush();auto pixels=MakeCharacterSheet(color);GLuint tex=0;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,640,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        it=sprites.emplace(color,tex).first;
    }
    Texture(it->second);Color tint(0xffffff,alpha);
    float u=(frame%8)/8.f,v=(direction%8)/8.f;
    Vec2 a=feet+Vec2(-32,-72),b=a+Vec2(64,0),c=a+Vec2(64,80),d=a+Vec2(0,80);
    Push(a,{u,v},tint);Push(b,{u+.125f,v},tint);Push(c,{u+.125f,v+.125f},tint);
    Push(a,{u,v},tint);Push(c,{u+.125f,v+.125f},tint);Push(d,{u,v+.125f},tint);
}
void TutorialRenderer::Triangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    Texture(white); Push(a, {}, color); Push(b, {}, color); Push(c, {}, color);
}
void TutorialRenderer::Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) {
    Triangle(a,b,c,color); Triangle(a,c,d,color);
}
void TutorialRenderer::Rect(float x, float y, float w, float h, Color c) { Quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},c); }
void TutorialRenderer::Ellipse(Vec2 p, float rx, float ry, Color c, int segments) {
    for (int i=0; i<segments; ++i) {
        float a = i * 6.2831853f / segments, b = (i+1) * 6.2831853f / segments;
        Triangle(p, p+Vec2(std::cos(a)*rx,std::sin(a)*ry), p+Vec2(std::cos(b)*rx,std::sin(b)*ry), c);
    }
}
void TutorialRenderer::Line(Vec2 a, Vec2 b, float width, Color c) {
    Vec2 d = b-a; float len = std::sqrt(d.x*d.x+d.y*d.y); if (len < .001f) return;
    Vec2 n(-d.y/len*width*.5f, d.x/len*width*.5f); Quad(a+n,b+n,b-n,a-n,c);
}
TutorialRenderer::Label TutorialRenderer::MakeLabel(const std::wstring& text, int size) {
    Label result;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return result;
    HFONT font = CreateFontW(-size,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc,font);
    SIZE extent = {}; GetTextExtentPoint32W(dc,text.c_str(),static_cast<int>(text.size()),&extent);
    result.width = (std::max)(1,static_cast<int>(extent.cx)+4); result.height = (std::max)(size+8,static_cast<int>(extent.cy)+4);
    BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=result.width; info.bmiHeader.biHeight=-result.height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits = nullptr; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (bitmap && bits) {
        HGDIOBJ oldBitmap=SelectObject(dc,bitmap);
        PatBlt(dc,0,0,result.width,result.height,BLACKNESS);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
        TextOutW(dc,1,1,text.c_str(),static_cast<int>(text.size())); GdiFlush();
        const unsigned char* src=static_cast<unsigned char*>(bits);
        std::vector<unsigned char> pixels(result.width*result.height*4);
        for (size_t i=0;i<pixels.size();i+=4) {
            pixels[i]=pixels[i+1]=pixels[i+2]=255;
            pixels[i+3]=(std::max)(src[i],(std::max)(src[i+1],src[i+2]));
        }
        glGenTextures(1,&result.texture); glBindTexture(GL_TEXTURE_2D,result.texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,result.width,result.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        SelectObject(dc,oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    SelectObject(dc,oldFont); if (font) DeleteObject(font); DeleteDC(dc);
    return result;
}
void TutorialRenderer::Text(float x, float y, const std::wstring& text, int size, Color color) {
    auto key=std::make_pair(size,text);
    auto found=labels.find(key);
    if (found==labels.end()) {
        Flush();
        // Bound the cache even when timer/health strings change during a long session.
        if (labels.size()>=512) {
            for (auto& entry:labels) glDeleteTextures(1,&entry.second.texture);
            labels.clear(); active=white;
        }
        found=labels.emplace(key,MakeLabel(text,size)).first;
    }
    const Label& l=found->second; if (!l.texture) return;
    Texture(l.texture);
    Vec2 a(x,y),b(x+l.width,y),c(x+l.width,y+l.height),d(x,y+l.height);
    Push(a,{0,0},color); Push(b,{1,0},color); Push(c,{1,1},color);
    Push(a,{0,0},color); Push(c,{1,1},color); Push(d,{0,1},color);
}
void TutorialRenderer::TextBlock(float x,float y,int maxWidth,const std::wstring& text,int size,Color color) {
    auto key=std::make_pair(size,std::to_wstring(maxWidth)+L":"+text);
    auto found=wrapped.find(key);
    if(found==wrapped.end()) {
        std::vector<std::wstring> lines;
        HDC dc=CreateCompatibleDC(nullptr);
        if(!dc) { Text(x,y,text,size,color); return; }
        HFONT font=CreateFontW(-size,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        HGDIOBJ old=SelectObject(dc,font);
        size_t offset=0;
        while(offset<text.size()) {
            int count=0; SIZE extent={};
            GetTextExtentExPointW(dc,text.c_str()+offset,static_cast<int>(text.size()-offset),maxWidth,&count,nullptr,&extent);
            count=(std::max)(1,count);
            if(offset+count<text.size()) {
                int space=count-1;
                while(space>count/2 && text[offset+space]!=L' ') --space;
                if(text[offset+space]==L' ') count=space;
            }
            lines.push_back(text.substr(offset,count)); offset+=count;
            while(offset<text.size() && text[offset]==L' ') ++offset;
        }
        SelectObject(dc,old); DeleteObject(font); DeleteDC(dc);
        if(wrapped.size()>256) wrapped.clear();
        found=wrapped.emplace(key,std::move(lines)).first;
    }
    for(const auto& line:found->second) { Text(x,y,line,size,color); y+=size+9; }
}
}
