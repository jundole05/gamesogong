#pragma once
#include "TutorialRenderer.h"
#include <vector>

namespace tutorial {
class TutorialGame {
public:
    void Reset();
    void Update(float dt);
    void Draw(TutorialRenderer& renderer);
    void KeyDown(unsigned char key);
    void KeyUp(unsigned char key);
    void ClearInput();
    void Attack();
private:
    struct NPC { Vec2 p; std::wstring name, role; unsigned color; };
    struct Prop { Vec2 p; int type; float w,h; unsigned color; };
    struct Enemy { Vec2 p,home,aim; int hp; float windup,cooldown,flash; };
    struct Dialogue { std::wstring speaker; std::vector<std::wstring> lines; int action; };
    struct Dust { Vec2 p; float life; };
    std::vector<Dust> dust;
    float dustTimer=0;
    bool keys[256] = {};
    Vec2 player{4,9}, camera{4,9}, facing{1,0}, dashDirection{1,0};
    std::vector<NPC> npcs;
    std::vector<Prop> props;
    std::vector<Enemy> enemies;
    Dialogue dialogue;
    int page=0, stage=0, hp=100, choice=3, deaths=0;
    bool intro=true, paused=false, journal=false, finished=false, speaking=false;
    bool arkel=false, varka=false, dodged=false, restored=false, moving=false;
    float clock=0, elapsed=0, dash=0, dashCooldown=0, attackCooldown=0, slash=0, invulnerable=0, noticeTime=0;
    std::wstring notice;
    Vec2 Project(Vec2 p, float height=0) const;
    Vec2 Goal() const;
    bool Blocked(Vec2 p) const;
    void Move(Vec2 delta);
    void Dodge();
    void Interact();
    void Talk(int index);
    void Say(std::wstring name, std::vector<std::wstring> lines, int action=0);
    void AdvanceDialogue();
    void Notify(std::wstring text);
    void Terrain(TutorialRenderer& r);
    void DrawProp(TutorialRenderer& r, const Prop& prop);
    void Person(TutorialRenderer& r, Vec2 p, unsigned color, bool hero=false, bool enemy=false);
    void Interface(TutorialRenderer& r);
    void Panel(TutorialRenderer& r, float x,float y,float w,float h);
};
}
