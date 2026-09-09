/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed WITHOUT ANY WARRANTY.
*/
#include "stdafx.h"
#include "Dependencies/glew.h"
#include "Dependencies/freeglut.h"
#include "TutorialRenderer.h"
#include "TutorialGame.h"
#include <windows.h>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {
tutorial::TutorialRenderer renderer;
tutorial::TutorialGame game;
bool ready=false;
using Clock=std::chrono::steady_clock;
Clock::time_point previous;

void Display() { if(ready) { game.Draw(renderer); glutSwapBuffers(); } }
void Resize(int w,int h) { renderer.Resize(w,h); }
void KeyDown(unsigned char k,int,int) { game.KeyDown(k); }
void KeyUp(unsigned char k,int,int) { game.KeyUp(k); }
void Mouse(int button,int state,int,int) {
    if(button==GLUT_LEFT_BUTTON && state==GLUT_DOWN) game.Attack();
}
void Close() { if(ready) { renderer.Shutdown(); ready=false; } }
void Tick(int) {
    if(!ready) return;
    auto now=Clock::now(); float dt=std::chrono::duration<float>(now-previous).count(); previous=now;
    DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process==GetCurrentProcessId()) game.Update((std::min)(dt,.05f));
    else game.ClearInput();
    glutPostRedisplay(); glutTimerFunc(16,Tick,0);
}
}
int main(int argc,char** argv) {
    SetProcessDPIAware();
    glutInit(&argc,argv);
    glutInitContextVersion(3,3); glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(1280,720); glutInitWindowPosition(70,50);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutCreateWindow("Ermarin");
    // FreeGLUT's title API is ANSI; replace it via Win32 to preserve Korean text.
    SetWindowTextW(GetActiveWindow(),L"에르마린의 잔광 — 여울목의 등불");
    glewExperimental=GL_TRUE;
    GLenum status=glewInit();
    if(status!=GLEW_OK || !GLEW_VERSION_3_3) {
        MessageBoxW(nullptr,L"이 프로토타입은 OpenGL 3.3 이상을 지원하는 그래픽 드라이버가 필요합니다.",L"그래픽 초기화 실패",MB_OK|MB_ICONERROR);
        return 1;
    }
    while(glGetError()!=GL_NO_ERROR) {}
    if(!renderer.Initialize()) {
        MessageBoxW(nullptr,L"셰이더를 초기화하지 못했습니다. 콘솔의 그래픽 오류를 확인해 주세요.",L"렌더러 초기화 실패",MB_OK|MB_ICONERROR);
        return 1;
    }
    ready=true; game.Reset(); renderer.Resize(1280,720); previous=Clock::now();
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display); glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown); glutKeyboardUpFunc(KeyUp);
    glutMouseFunc(Mouse); glutCloseFunc(Close);
    glutTimerFunc(16,Tick,0); glutMainLoop(); Close();
    return 0;
}
