#include "stdafx.h"
#include "TutorialGame.h"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace tutorial {
namespace {
float Length(Vec2 p) { return std::sqrt(p.x*p.x+p.y*p.y); }
Vec2 Unit(Vec2 p) { float n=Length(p); return n>.0001f ? p*(1/n) : Vec2(); }
float Clamp(float v,float lo,float hi) { return (std::max)(lo,(std::min)(v,hi)); }
unsigned Shade(unsigned c,float f) {
    return (unsigned(Clamp(((c>>16)&255)*f,0,255))<<16) |
           (unsigned(Clamp(((c>>8)&255)*f,0,255))<<8) | unsigned(Clamp((c&255)*f,0,255));
}
constexpr int mapWidth=56, mapHeight=36;
constexpr float forestEdge=28, shoreY=30;
const Vec2 relic(44,16);
}
void TutorialGame::Reset() {
    ClearInput(); player={8,18}; camera={11,16}; facing={1,0}; dashDirection=facing;
    stage=0; hp=100; choice=3; deaths=0; page=0;
    intro=true; paused=journal=finished=speaking=false;
    arkel=varka=dodged=restored=moving=false;
    clock=elapsed=dash=dashCooldown=attackCooldown=slash=invulnerable=noticeTime=0;
    dialogue={}; notice.clear();
    dust.clear();dustTimer=0;
    npcs={
        {{5.5f,7.6f},L"미라",L"국경 마을의 촌장",0xc88b56},
        {{8,9.3f},L"레온",L"아르켈 연방 · 조사관",0x568bb8},
        {{10,7},L"사하",L"바르카 연맹 · 길잡이",0xc97b58},
        {{3,6.5f},L"도란",L"빵을 굽는 상인",0xdcb976},
        {{6,11},L"나리",L"축제를 기다리는 아이",0xa9ba76},
        {{11,10},L"오렌",L"전쟁에서 돌아온 목수",0x888d96},
        {{3,10.5f},L"예나",L"마을의 약초사",0x739e8a}
    };
    props={
        {{3,3.7f},0,2.0f,1.55f,0x557c86}, {{7,3.5f},0,2.1f,1.6f,0x976451},
        {{10.5f,3.5f},0,1.6f,1.4f,0x6d7d69}, {{3,12.7f},0,1.6f,1.1f,0x98694e},
        {{8,13},0,1.7f,1.1f,0x586b88},
        {{2.4f,6},3,1.2f,.6f,0xc99869}, {{5,5.4f},2,.2f,.2f,0x689ccd},
        {{10,5.4f},2,.2f,.2f,0xd58e62}, {{5.5f,9.5f},4,.2f,.2f,0xffbb69},
        {{12.8f,6.8f},5,.4f,.4f,0x88948c}, {{12.8f,9.3f},5,.4f,.4f,0x88948c},
        {relic,6,.55f,.55f,0xb784e6}
    };
    for(auto& n:npcs)n.p=n.p*2;
    for(auto& p:props)if(p.type!=6)p.p=p.p*2;
    for(int x=1;x<mapWidth-1;++x) for(int y=1;y<30;++y) {
        if ((x*17+y*31)%19!=0 || (y>=12 && y<=21) || (x<26 && y>4 && y<28)) continue;
        props.push_back({Vec2(float(x),float(y)),1,.34f,.34f,x<forestEdge?0x4c7962u:0x3f5361u});
    }
    props.push_back({{32,11},1,.34f,.34f,0x354b5a});
    props.push_back({{40,22},1,.34f,.34f,0x4c5065});
    enemies={{{35,16},{35,16},{},40,0,.7f,0},{{40,13.6f},{40,13.6f},{},40,0,1.1f,0},
             {{42,19.6f},{42,19.6f},{},40,0,1.4f,0}};
}
void TutorialGame::ClearInput() { for(bool& k:keys) k=false; moving=false; }
Vec2 TutorialGame::Project(Vec2 p,float height) const {
    Vec2 d=p-camera; return {640+(d.x-d.y)*39,355+(d.x+d.y)*19.5f-height};
}
Vec2 TutorialGame::Goal() const {
    if(stage==0 || stage==5 || stage==6) return npcs[0].p;
    if(stage==1) return !arkel?npcs[1].p:npcs[2].p;
    if(stage==2) return {26,16};
    if(stage==3) { for(const auto& e:enemies) if(e.hp>0) return e.p; }
    return relic;
}
bool TutorialGame::Blocked(Vec2 p) const {
    if(p.x<.6f || p.x>mapWidth-.6f || p.y<.6f || p.y>shoreY-.5f) return true;
    for(const auto& prop:props) {
        if(prop.type==2 || prop.type==4) continue;
        if(std::abs(p.x-prop.p.x)<prop.w+.20f && std::abs(p.y-prop.p.y)<prop.h+.20f) return true;
    }
    return false;
}
void TutorialGame::Move(Vec2 delta) {
    // Substeps prevent dashes tunnelling through a tree or building footprint.
    int steps=(std::max)(1,int(Length(delta)/.10f)+1); delta=delta*(1.f/steps);
    for(int i=0;i<steps;++i) {
        Vec2 p=player+Vec2(delta.x,0); if(!Blocked(p)) player=p;
        p=player+Vec2(0,delta.y); if(!Blocked(p)) player=p;
    }
}
void TutorialGame::Notify(std::wstring text) { notice=std::move(text); noticeTime=4; }
void TutorialGame::Say(std::wstring name,std::vector<std::wstring> lines,int action) {
    dialogue={std::move(name),std::move(lines),action}; page=0; speaking=true; ClearInput();
}
void TutorialGame::AdvanceDialogue() {
    if(++page<static_cast<int>(dialogue.lines.size())) return;
    int action=dialogue.action; speaking=false; ClearInput();
    if(action==1) stage=1;
    if(action==2) arkel=true;
    if(action==3) varka=true;
    if(stage==1 && arkel && varka) { stage=2; Notify(L"두 진영의 도움이 모였습니다. 동쪽 길에서 Space로 회피하세요."); }
    if(action==4) {
        restored=true; stage=5; hp=100;
        npcs[1].p={13,19}; npcs[2].p={16,19};
        Notify(L"숲에 온기가 돌아옵니다. 마을의 미라에게 돌아가세요.");
    }
    if(action==5) { stage=6; finished=true; }
}
void TutorialGame::Talk(int i) {
    if(i==0) {
        if(stage==0) Say(L"미라 · 국경 마을의 촌장",{
            L"여기는 두 진영이 함께 사는 여울목이야. 회복력 27년, 휴전도 스물일곱 해째지.",
            L"아르켈은 별의 힘으로 도시를 세웠고, 바르카는 그 힘이 숲을 해친다고 경고했어.",
            L"전쟁은 끝났지만 숲에 그림자병이 돌아왔어. 오늘의 등불 축제도 미뤄야 할지 몰라.",
            L"레온과 사하를 만나 줘. 서로 다투고 있어도, 우리 마을을 지키고 싶은 마음은 같단다."
        },1);
        else if(stage==5) Say(L"미라 · 다시 켜진 등불",{
            L"돌아왔구나! 숲의 빛이 달라졌어. 레온과 사하도 다친 사람들을 함께 돌보고 있단다.",
            L"오렌은 전쟁에서 잃은 동생의 등불을 만들었어. 나리는 그 옆에 새 꽃을 달아 줬고.",
            L"우리가 모두 용서한 건 아니야. 그래도 오늘 저녁은 같은 식탁에 앉을 수 있겠지.",
            L"네가 지킨 건 작은 마을의 하루야. 에르마린의 긴 여행은 여기서 시작된단다."
        },5);
        else if(stage==6) Say(L"미라",{L"등불이 켜졌구나. 이 마을은 네가 돌아올 자리를 언제나 남겨 둘 거야."});
        else Say(L"미라",{L"레온의 기술과 사하의 지혜를 함께 빌려 보렴. 다친 채 돌아오면 예나가 도와줄 거야."});
    } else if(i==1) {
        if(restored) Say(L"레온 · 아르켈 연방",{L"사하가 없었다면 숲의 뿌리까지 지키지는 못했을 겁니다. 조사 기록에도 그렇게 쓰겠습니다.",L"내일은 연맹의 약초를 함께 연구하기로 했습니다. 오늘은... 같이 빵부터 먹고요."});
        else if(stage==0) Say(L"레온",{L"숲의 균열을 조사하러 왔습니다. 먼저 미라 촌장님께 상황을 들어 주세요."});
        else Say(L"레온 · 아르켈 연방",{
            L"세라프는 하늘에서 떨어진 별입니다. 그 잔해의 마력으로 우리는 약과 빛을 만들었지요.",
            L"전쟁에서는 그 힘을 무기로도 썼습니다. 우리 연방 역시 책임을 피할 수 없습니다.",
            L"이 공명기를 가져가세요. 사하의 봉인 문양과 연결하면 숲의 잔해를 안정시킬 수 있습니다.",
            L"J 또는 마우스 왼쪽 버튼으로 검을 휘두르세요. 가까운 그림자를 향해 공격합니다."
        },stage==1?2:0);
    } else if(i==2) {
        if(restored) Say(L"사하 · 바르카 연맹",{L"레온이 책임을 기록하겠다고 하더군. 말뿐인지 지켜볼 거야. 그래도 시작은 나쁘지 않아.",L"오렌과 등불을 만들었어. 잃은 이들을 기억하는 방식은 우리도 다르지 않더라."});
        else if(stage==0) Say(L"사하",{L"여기는 내 부족만의 땅이 아니야. 미라에게 먼저 이야기를 들어 봐."});
        else Say(L"사하 · 바르카 연맹",{
            L"바르카는 숲과 함께 살아. 잔해를 캐지 말라고 했지만, 우리도 전쟁에서 그 힘을 썼어.",
            L"레온이 가져온 기계는 못 미더워. 하지만 지난겨울 그의 약이 우리 아이들을 살렸지.",
            L"이 봉인 문양을 가져가. 동쪽 숲의 그림자가 사라지면 잔해 가까이에서 E를 눌러.",
            L"적의 붉은 원이 차오르면 Space로 회피해. 회피하는 동안은 공격을 피할 수 있어."
        },stage==1?3:0);
    } else if(i==3) Say(L"도란 · 빵 굽는 상인",{L"연방 밀가루와 연맹 꿀로 구운 빵이야. 다투던 손님들도 이 냄새 앞에서는 조용하지.",L"축제가 열리면 첫 빵은 네 몫이다. 무사히 돌아와!"});
    else if(i==4) Say(L"나리 · 마을 아이",{L"파란 깃발도 붉은 깃발도 예쁜데, 어른들은 왜 한쪽만 좋아하래?",L"아저씨가 슬퍼해서 꽃을 줬어. 오늘은 다 같이 등불을 날렸으면 좋겠어."});
    else if(i==5) Say(L"오렌 · 목수",{L"동생은 저 국경 너머에서 돌아오지 못했어. 사하를 보면 아직도 마음이 편하진 않아.",L"그런데 어제 사하가 내 지붕을 고쳐 줬지. 고맙다는 말은... 내일쯤 할 수 있을 것 같아."});
    else { hp=100; Say(L"예나 · 약초사",{L"상처를 치료했어요. 우리 약초에 연방의 정제 기술을 더한 약이랍니다.",L"길에서 쓰러져도 여기서 다시 일어날 수 있어요. 이미 도운 사람들은 잊지 않아요."}); }
}
void TutorialGame::Interact() {
    if(stage==4 && Length(player-relic)<2.1f) {
        Say(L"세라프의 잔해 · 잔광을 듣는 힘",{
            L"차가운 별빛 사이로 오래전 전장의 목소리가 들린다. 두 진영의 무기가 같은 빛을 삼켰다.",
            L"레온의 공명기와 사하의 문양이 반응한다. 누구의 잘못을 묻기 전에, 지금의 생명을 지킬 때다.",
            L"1  공명기로 마력을 조절한다.     2  문양으로 땅을 봉인한다.     3  두 방법을 연결한다."
        },4); return;
    }
    int nearest=-1; float distance=1.65f;
    for(int i=0;i<static_cast<int>(npcs.size());++i) {
        float d=Length(player-npcs[i].p); if(d<distance) { nearest=i; distance=d; }
    }
    if(nearest>=0) Talk(nearest);
    else Notify(L"주민 또는 정화할 잔해 가까이에서 E를 누르세요.");
}
void TutorialGame::Dodge() {
    if(dashCooldown>0) return;
    dash=.20f; dashCooldown=1.05f; dashDirection=facing; invulnerable=.30f; dodged=true;
}
void TutorialGame::Attack() {
    if(intro || paused || journal || finished || speaking || attackCooldown>0 || dash>0) return;
    attackCooldown=.38f; slash=.22f;
    Enemy* target=nullptr; float nearest=1.9f;
    if(stage==3) for(auto& e:enemies) if(e.hp>0 && Length(e.p-player)<nearest) { target=&e; nearest=Length(e.p-player); }
    if(target) facing=Unit(target->p-player);
    if(stage==3) for(auto& e:enemies) {
        Vec2 d=e.p-player;
        if(e.hp>0 && Length(d)<1.9f && (d.x*facing.x+d.y*facing.y)>-.15f) {
            e.hp-=20; e.flash=.18f;
            if(e.hp<=0) e.windup=0;
        }
    }
}
void TutorialGame::KeyDown(unsigned char raw) {
    unsigned char k=static_cast<unsigned char>(std::tolower(raw));
    if(keys[k]) return; keys[k]=true;
    if(intro) { if(k==13 || k=='e') { intro=false; ClearInput(); } return; }
    if(finished) { if(k=='r') Reset(); else if(k==13) { finished=false; ClearInput(); } return; }
    if(k==27) { paused=!paused; ClearInput(); return; }
    if(paused) { if(k=='r') Reset(); return; }
    if(k==9) { journal=!journal; ClearInput(); return; }
    if(journal) return;
    if(speaking) {
        bool choosing=dialogue.action==4 && page==static_cast<int>(dialogue.lines.size())-1;
        if(choosing) { if(k>='1' && k<='3') { choice=k-'0'; AdvanceDialogue(); } }
        else if(k=='e' || k==13) AdvanceDialogue();
        return;
    }
    if(k=='e') Interact();
    if(k==' ') Dodge();
    if(k=='j') Attack();
}
void TutorialGame::KeyUp(unsigned char k) { keys[static_cast<unsigned char>(std::tolower(k))]=false; }
void TutorialGame::Update(float dt) {
    clock+=dt;
    if(intro || paused || journal || finished) return;
    elapsed+=dt;
    noticeTime=(std::max)(0.f,noticeTime-dt);
    if(speaking) return;
    dashCooldown=(std::max)(0.f,dashCooldown-dt); attackCooldown=(std::max)(0.f,attackCooldown-dt);
    slash=(std::max)(0.f,slash-dt); invulnerable=(std::max)(0.f,invulnerable-dt);
    // Invert the isometric projection so WASD follows screen directions.
    float sx=float(keys['d'])-float(keys['a']), sy=float(keys['s'])-float(keys['w']);
    Vec2 direction=Unit(Vec2(sx+sy*2,sy*2-sx)); moving=Length(direction)>.1f;
    if(dash>0) { Move(dashDirection*(11.f*dt)); dash=(std::max)(0.f,dash-dt); }
    else if(moving) { facing=direction; Move(direction*(5.2f*dt)); }
    camera=camera+(player-camera)*(1-std::exp(-5*dt));
    for(auto& d:dust)d.life-=dt;
    dust.erase(std::remove_if(dust.begin(),dust.end(),[](const Dust& d){return d.life<=0;}),dust.end());
    dustTimer-=dt;
    if((moving || dash>0) && dustTimer<=0){dust.push_back({player,.65f});dustTimer=.10f;}
    if(stage==2 && dodged && player.x>26) { stage=3; Notify(L"그림자 3마리를 물리치세요. 붉은 공격 예고를 보고 회피하세요."); }
    if(stage==3) {
        for(auto& e:enemies) {
            e.flash=(std::max)(0.f,e.flash-dt); if(e.hp<=0) continue;
            e.cooldown=(std::max)(0.f,e.cooldown-dt);
            if(e.windup>0) {
                e.windup-=dt;
                if(e.windup<=0) {
                    if(Length(player-e.aim)<1.3f && invulnerable<=0) { hp-=18; invulnerable=.65f; Notify(L"붉은 원 밖으로 피하세요! Space 회피 중에는 피해를 받지 않습니다."); }
                    e.cooldown=1.25f;
                }
            } else if(Length(player-e.p)<1.3f && e.cooldown<=0) { e.aim=player; e.windup=.75f; }
            else if(Length(player-e.p)<6 && Length(player-e.p)>1.0f) {
                Vec2 next=e.p+Unit(player-e.p)*(1.5f*dt);
                if(next.x>27 && !Blocked(next)) e.p=next;
            }
        }
        if(hp<=0) {
            ++deaths; hp=100; player={7.6f,20}; camera=player; invulnerable=2; dash=0;
            for(auto& e:enemies) { e.p=e.home; e.windup=0; e.cooldown=1; }
            Notify(L"예나가 당신을 치료했습니다. 처치한 그림자와 퀘스트 진행은 유지됩니다.");
        }
        bool all=true; for(const auto& e:enemies) if(e.hp>0) all=false;
        if(all) { stage=4; Notify(L"숲이 조용해졌습니다. 빛나는 잔해에 다가가 E로 정화하세요."); }
    }
}

void TutorialGame::Terrain(TutorialRenderer& r) {
    r.Rect(0,0,1280,720,Color(0x142831));
    for(int sum=0;sum<mapWidth+mapHeight-1;++sum) for(int x=0;x<mapWidth;++x) {
        int y=sum-x; if(y<0 || y>=mapHeight) continue;
        Vec2 p=Project({float(x),float(y)});
        if(p.x<-50 || p.x>1330 || p.y<-50 || p.y>770)continue;
        bool forest=x>=forestEdge, water=y>=shoreY, road=(y>=14 && y<=18) || (x>=10 && x<=12 && y>8 && y<26);
        unsigned c=water?0x294d59:road?(forest?0x66645f:0x9b9475):forest?(restored?0x4d715b:0x394e51):0x63795c;
        float variation=.93f+((x*13+y*7)%9)*.015f;
        r.MaterialQuad(p+Vec2(0,-19.5f),p+Vec2(39,0),p+Vec2(0,19.5f),p+Vec2(-39,0),Color(Shade(c,variation)),water?3:road?2:1,{float(x),float(y)},{1,1});
        if(road && (x+y)%2==0) r.Line(p+Vec2(-22,1),p+Vec2(6,15),1,Color(0xddd2ad,.19f));
        if(water) r.Line(p+Vec2(-16,std::sin(clock+x)*2),p+Vec2(14,0),1.5f,Color(0x97c4bb,.25f));
        if(!road && !water && (x*11+y*3)%4==0) {
            r.Line(p,p+Vec2(-3,-6),1,Color(forest?0x819499:0xa2b878,.6f));
            r.Line(p+Vec2(2,1),p+Vec2(5,-4),1,Color(0xa2b878,.5f));
            if(!forest && (x+y)%3==0) r.Ellipse(p+Vec2(-2,-5),2,2,Color(0xebc78d),6);
        }
    }
    // Mossy foundation along the water and a path of stepping stones.
    for(int x=0;x<mapWidth;++x) {
        Vec2 p=Project({float(x),shoreY-.3f});
        r.Quad(p+Vec2(-28,-8),p+Vec2(0,-19),p+Vec2(29,-3),p+Vec2(0,9),Color(0x677b70));
        float foam=.25f+.12f*std::sin(clock*1.4f+x);
        r.Line(p+Vec2(0,13),p+Vec2(31,0),2,Color(0xb5ded0,foam));
        if(x%3==0) for(int j=0;j<3;++j)r.Line(p+Vec2(j*4.f,0),p+Vec2(j*4.f+std::sin(clock+x)*3,-16-j*4.f),2,Color(0x86916b));
    }
    for(const auto& e:enemies) if(e.hp>0 && e.windup>0 && stage==3) {
        Vec2 p=Project(e.aim);
        r.Ellipse(p,65,32,Color(0xee665b,.20f));
        r.Ellipse(p,65*(1-e.windup/.75f),32*(1-e.windup/.75f),Color(0xff8a62,.38f));
    }
    Vec2 g=Project(Goal());
    r.Ellipse(g,28+std::sin(clock*3)*3,13,Color(0xf0d28a,.18f));
    for(int i=0;i<5;++i) {
        Vec2 p=Project(player+(Goal()-player)*((i+1)/6.f));
        if(Length(Goal()-player)>3) r.Ellipse(p,3,1.5f,Color(0xf4d99a,.5f));
    }
}
void TutorialGame::DrawProp(TutorialRenderer& r,const Prop& prop) {
    Vec2 p=Project(prop.p);
    if(p.x<-220 || p.x>1500 || p.y<-150 || p.y>980) return;
    if(prop.type==0) {
        // Footprint corners, extruded walls and pitched roof, sorted by ground depth.
        float alpha=(Length(player-prop.p)<4 && player.x+player.y<prop.p.x+prop.p.y+1)? .55f:1.f;
        Vec2 a=Project(prop.p+Vec2(-prop.w,-prop.h)),b=Project(prop.p+Vec2(prop.w,-prop.h));
        Vec2 c=Project(prop.p+Vec2(prop.w,prop.h)),d=Project(prop.p+Vec2(-prop.w,prop.h));
        Vec2 up(0,-83), ridge(0,-128);
        r.MaterialQuad(d,c,c+up,d+up,Color(0xa99375,alpha),4,{0,0},{1,1});
        r.MaterialQuad(c,b,b+up,c+up,Color(0x746e60,alpha),4,{0,0},{1,1});
        Vec2 topA=Project(prop.p+Vec2(-prop.w,0))+ridge, topB=Project(prop.p+Vec2(prop.w,0))+ridge;
        r.MaterialQuad(d+up+Vec2(-8,0),c+up+Vec2(8,0),topB,topA,Color(prop.color,alpha),5,{0,0},{1,1});
        r.MaterialQuad(topA,topB,b+up+Vec2(7,0),a+up+Vec2(-7,0),Color(Shade(prop.color,.74f),alpha),5,{0,0},{1,1});
        r.Triangle(c+up,b+up,topB,Color(0xbaa083,alpha));
        r.Line(d+up,c+up,4,Color(0x443f37,alpha));
        r.Line(c,c+up,5,Color(0x4e5148,alpha));
        for(int i=1;i<4;++i){float t=i/4.f;Vec2 base=d*(1-t)+c*t;
            r.Line(base,base+up,5,Color(0x635545,alpha));}
        r.Line(d+up*.48f,c+up*.48f,5,Color(0x695c47,alpha));
        for(int i=1;i<5;++i) {
            float t=i/5.f; r.Line((d+up)*(1-t)+topA*t,(c+up)*(1-t)+topB*t,1,Color(0xf0cfaa,.16f*alpha));
        }
        Vec2 door=(c+d)*.5f; r.Rect(door.x-11,door.y-43,22,39,Color(0x42483e,alpha));
        r.MaterialQuad(door+Vec2(-11,-43),door+Vec2(11,-43),door+Vec2(11,-4),door+Vec2(-11,-4),Color(0x907451,alpha),6,{0,0},{1,1});
        r.Rect(door.x-7,door.y-38,14,19,Color(0xf4bd69,alpha));
        r.SoftEllipse(door+Vec2(0,-26),30,33,Color(0xffc879,.18f*alpha));
        Vec2 win=(c+b)*.5f; r.Rect(win.x-10,win.y-52,20,25,Color(0x3c4542,alpha));
        r.Rect(win.x-7,win.y-49,14,19,Color(0xd3b56b,alpha));
        r.Line(win+Vec2(0,-49),win+Vec2(0,-30),2,Color(0x60513e,alpha));
        r.Rect(topA.x+28,topA.y-19,13,31,Color(0x626862,alpha));
        for(int i=0;i<3;++i) r.Ellipse(topA+Vec2(35+std::sin(clock+i)*8,-30-i*13),9+i*3.f,7+i*2.f,Color(0xb9c5ba,.08f));
    } else if(prop.type==1) {
        float height=88.f+static_cast<float>(int(prop.p.x*7+prop.p.y*11)%32);
        r.Quad(p+Vec2(-6,0),p+Vec2(6,0),p+Vec2(4,-height),p+Vec2(-3,-height),Color(0x574e48));
        for(int i=0;i<3;++i) {
            float y=-height+28.f*i, w=24.f+10.f*i;
            r.Triangle(p+Vec2(0,y-27),p+Vec2(-w,y+29),p+Vec2(w,y+29),Color(Shade(prop.color,1-i*.06f)));
            r.Triangle(p+Vec2(0,y-27),p+Vec2(0,y+29),p+Vec2(w,y+29),Color(Shade(prop.color,.79f-i*.04f)));
            for(int j=0;j<6;++j){float dx=(j-2.5f)*w*.27f+std::sin(clock+prop.p.x+i)*2;
                r.Line(p+Vec2(dx,y+13),p+Vec2(dx+7,y+20),1.3f,Color(0x9cae87,.18f));}
        }
    } else if(prop.type==2) {
        r.Line(p,p+Vec2(0,-104),4,Color(0x594f43));
        float wave=std::sin(clock*2+prop.p.x)*4;
        r.Quad(p+Vec2(2,-100),p+Vec2(31,-94+wave),p+Vec2(29,-57+wave),p+Vec2(2,-65),Color(prop.color));
        r.Triangle(p+Vec2(15,-86),p+Vec2(9,-74),p+Vec2(23,-74),Color(0xf0dfb4));
        r.Ellipse(p+Vec2(0,-105),4,4,Color(0xd9bb7e),8);
    } else if(prop.type==3) {
        r.Rect(p.x-40,p.y-24,80,27,Color(0x695241));
        r.Quad(p+Vec2(-48,-32),p+Vec2(24,-54),p+Vec2(55,-33),p+Vec2(-17,-11),Color(0xc58b5c));
        for(int i=0;i<6;++i) r.Ellipse(p+Vec2(-25+i*10.f,-28),6,4,Color(i%2?0xddaf63:0xab684b),8);
    } else if(prop.type==4) {
        r.SoftEllipse(p,110,53,Color(0xffbf61,.25f));
        r.Ellipse(p,19,9,Color(0x4a4941));
        r.Line(p+Vec2(-12,0),p+Vec2(12,-4),5,Color(0x79583c));
        r.Triangle(p+Vec2(-9,-3),p+Vec2(3,-30-std::sin(clock*9)*4),p+Vec2(12,-3),Color(0xeaa357));
        r.Triangle(p+Vec2(-5,-3),p+Vec2(2,-22),p+Vec2(6,-3),Color(0xffe8a2));
    } else if(prop.type==5) {
        r.Rect(p.x-13,p.y-65,26,65,Color(0x727c72));
        r.Rect(p.x,p.y-65,13,65,Color(0x536461));
        r.Quad(p+Vec2(-19,-65),p+Vec2(0,-75),p+Vec2(19,-65),p+Vec2(0,-55),Color(0xa0a18c));
        r.SoftEllipse(p+Vec2(0,-72),38,43,Color(0xf1c779,.28f));
        r.Rect(p.x-5,p.y-81,10,15,Color(0xe4b871));
    } else {
        unsigned glow=restored?0x91dbbc:0xb882e3;
        r.SoftEllipse(p,115,57,Color(glow,.32f)); r.Ellipse(p,47,23,Color(0x222f3b));
        for(int i=0;i<8;++i) {
            float a=i*6.2831853f/8;
            r.Ellipse(p+Vec2(std::cos(a)*39,std::sin(a)*19),7,4,Color(0x8a8c91),6);
        }
        float hover=std::sin(clock*2)*4;
        Vec2 c=p+Vec2(0,-30+hover);
        r.Triangle(c+Vec2(0,-46),c+Vec2(-19,0),c+Vec2(0,24),Color(glow));
        r.Triangle(c+Vec2(0,-46),c+Vec2(18,0),c+Vec2(0,24),Color(Shade(glow,.67f)));
        r.Line(c+Vec2(0,-43),c+Vec2(-15,0),2,Color(0xf1d6f5));
    }
}
void TutorialGame::Person(TutorialRenderer& r,Vec2 world,unsigned color,bool hero,bool enemy) {
    Vec2 p=Project(world);
    float bob=hero && moving?std::sin(clock*13)*2:std::sin(clock*2+world.x)*.8f;
    if(hero && dash>0) for(int i=1;i<4;++i) r.Ellipse(Project(world-dashDirection*(i*.25f),16),12,20,Color(0xc6ecde,.12f));
    Color coat(color,hero && invulnerable>0 && int(clock*18)%2==0?.45f:1.f);
    if(enemy) {
        r.Ellipse(p+Vec2(0,-16+bob),16,18,coat,10);
        r.Triangle(p+Vec2(-13,-22),p+Vec2(-22,-40),p+Vec2(-2,-29),Color(0x646075));
        r.Triangle(p+Vec2(13,-22),p+Vec2(22,-40),p+Vec2(2,-29),Color(0x646075));
        r.Ellipse(p+Vec2(-6,-22),3,2,Color(0xf7a0ae),8); r.Ellipse(p+Vec2(6,-22),3,2,Color(0xf7a0ae),8);
        return;
    }
    Vec2 look=hero?facing:Unit(player-world);
    float angle=std::atan2((look.x+look.y)*.5f,look.x-look.y);
    int direction=(int(std::floor(angle*8/6.2831853f+.5f))+8)%8;
    int frame=hero && moving?int(clock*12)%8:0;
    r.Character(p+Vec2(0,hero?(dash>0?-3.f:0.f):bob),color,direction,frame,coat.a);
    if(hero) {
        float swing=slash>0?std::sin(slash/.22f*3.14159f)*20:0;
        r.Line(p+Vec2(16,-22),p+Vec2(26+swing,-49+swing),3,Color(0xdce6dc));
        r.Line(p+Vec2(15,-29),p+Vec2(24,-25),3,Color(0xceac66));
        if(slash>0) {
            Vec2 screen=Unit(Vec2(facing.x-facing.y,(facing.x+facing.y)*.5f));
            float a=std::atan2(screen.y,screen.x), sweep=(.22f-slash)/.22f;
            for(int i=0;i<14;++i) {
                float t=a-1.4f+sweep*.8f+i*.16f;
                Vec2 v(std::cos(t)*69,std::sin(t)*36);
                r.Line(p+Vec2(0,-20)+v,p+Vec2(0,-20)+Vec2(std::cos(t+.13f)*69,std::sin(t+.13f)*36),4,Color(0xf9e8ad,slash/.22f));
            }
        }
    }
}
void TutorialGame::Panel(TutorialRenderer& r,float x,float y,float w,float h) {
    r.Rect(x+4,y+6,w,h,Color(0x040d16,.25f)); r.Rect(x,y,w,h,Color(0x10212d,.94f));
    r.Rect(x,y,w,2,Color(0xcbb581,.75f)); r.Rect(x,y+h-1,w,1,Color(0x82918d,.3f));
}
void TutorialGame::Interface(TutorialRenderer& r) {
    Panel(r,24,22,312,120);
    r.Text(42,33,L"에르마린의 잔광",24,Color(0xf1d59c));
    r.Text(42,67,player.y>26?L"여울 호수 · 물안개 기슭":player.x<forestEdge?L"여울목 · 국경 마을":restored?L"별빛이 돌아온 숲":L"그림자 숲 · 세라프의 상처",16);
    r.Rect(43,103,202,8,Color(0x35444a)); r.Rect(43,103,202*hp/100.f,8,Color(0x8dc5a7));
    r.Text(255,94,std::to_wstring(hp)+L" / 100",14);
    r.Text(43,118,L"회복력 27년 · 등불 축제 전날",12,Color(0xa4b1b0));
    Panel(r,919,22,337,161);
    static const wchar_t* titles[]={L"01  국경 마을의 부탁",L"02  서로 다른 두 목소리",L"03  숲으로 가는 길",L"04  그림자와 맞서기",L"05  잔광을 듣는 힘",L"06  같은 식탁으로",L"여울목의 첫 번째 등불"};
    r.Text(940,37,titles[stage],19,Color(0xf0d79d));
    std::wstring goal;
    if(stage==0) goal=L"미라에게 다가가 E로 대화";
    else if(stage==1) goal=!arkel?L"레온의 이야기를 듣기 · E":L"사하의 이야기를 듣기 · E";
    else if(stage==2) goal=L"Space로 회피한 뒤 동쪽 숲으로";
    else if(stage==3) { int n=0;for(const auto& e:enemies)if(e.hp<=0)++n;goal=L"그림자 처치  "+std::to_wstring(n)+L" / 3 · J 공격"; }
    else if(stage==4) goal=L"별의 잔해 가까이에서 E로 정화";
    else if(stage==5) goal=L"마을로 돌아가 미라에게 보고 · E";
    else goal=L"튜토리얼 완료 · 자유롭게 둘러보기";
    r.Text(940,72,goal,16);
    for(int i=0;i<6;++i) r.Rect(941+i*48.f,110,40,4,Color(stage>i?0xd7bf83:0x3b505b));
    int total=int(elapsed); std::wstring sec=std::to_wstring(total%60); if(sec.size()<2)sec=L"0"+sec;
    r.Text(940,133,L"여정 "+std::to_wstring(total/60)+L":"+sec+L"  ·  예상 3~5분",14,Color(0xb1b9b2));
    Panel(r,24,658,1232,42);
    r.Text(42,669,L"WASD 이동    E 대화·조사    J / 마우스 왼쪽 공격    Space 회피    Tab 세계관    Esc 일시정지",16);
    if(dashCooldown>0) {
        r.Rect(574,626,132,5,Color(0x253b43)); r.Rect(574,626,132*(1-dashCooldown/1.05f),5,Color(0x8ccab8));
    } else r.Text(580,624,L"Space 회피 준비",13,Color(0xd3dec8));
    // Minimap uses world coordinates, with an explicit objective marker.
    Panel(r,1060,456,196,186); r.Text(1076,466,L"여울목과 동쪽 숲",14,Color(0xd6c599));
    r.Rect(1076,497,163,124,Color(0x304b4a)); r.Rect(1154,497,85,124,Color(restored?0x40634e:0x283642));
    r.Rect(1076,548,163,17,Color(0xa89975,.65f));
    r.Rect(1076,600,163,21,Color(0x356577));
    auto map=[](Vec2 p){return Vec2(1078+p.x*158.f/mapWidth,501+p.y*118.f/mapHeight);};
    for(const auto& n:npcs) r.Ellipse(map(n.p),2.5f,2.5f,Color(n.color),8);
    if(stage==3) for(const auto& e:enemies) if(e.hp>0) r.Ellipse(map(e.p),3,3,Color(0xe37e80),8);
    r.Ellipse(map(Goal()),5,5,Color(0xf3d38b,.7f),8); r.Ellipse(map(player),3,3,Color(0xffffff),8);
    Vec2 target=Project(Goal());
    if(target.x<65 || target.x>1020 || target.y<200 || target.y>580) {
        target.x=Clamp(target.x,65,1000);target.y=Clamp(target.y,205,575);
        r.Ellipse(target,16,16,Color(0x182c34,.85f));
        r.Text(target.x-7,target.y-13,L"◆",18,Color(0xf5d693));
    }
    if(!speaking && !intro) {
        for(const auto& n:npcs) if(Length(player-n.p)<1.65f) {
            Vec2 p=Project(n.p,122); Panel(r,p.x-84,p.y,180,32); r.Text(p.x-73,p.y+4,L"E  "+n.name+L"와 대화",15); break;
        }
        if(stage==4 && Length(player-relic)<2.1f) { Vec2 p=Project(relic,104);r.Text(p.x-66,p.y,L"E  잔해 정화",18,Color(0xf6d5ab)); }
    }
    if(noticeTime>0 && !speaking) { Panel(r,110,580,890,38);r.Text(125,588,notice,15); }
    if(speaking) {
        Panel(r,100,457,945,185);
        r.Text(126,473,dialogue.speaker,21,Color(0xf3d19a));
        r.TextBlock(126,517,890,dialogue.lines[page],18);
        bool choose=dialogue.action==4 && page==static_cast<int>(dialogue.lines.size())-1;
        r.Text(126,591,choose?L"1 / 2 / 3 선택 · 어떤 선택에도 두 진영의 도움이 필요합니다.":L"E / Enter 다음 이야기",15,Color(0xa9c4be));
        r.Text(942,591,std::to_wstring(page+1)+L" / "+std::to_wstring(dialogue.lines.size()),14);
    }
    if(intro || paused || journal || finished) {
        r.Rect(0,0,1280,720,Color(0x07131e,.70f)); Panel(r,210,135,860,450);
        if(intro) {
            r.Text(250,166,L"에르마린의 잔광",38,Color(0xf1d29b));
            r.Text(253,220,L"첫 번째 여정  /  여울목의 등불",20,Color(0xa9cbbc));
            r.Text(253,278,L"별이 떨어진 뒤, 사람들은 그 빛으로 도시를 세우고 전쟁을 벌였다.",20);
            r.TextBlock(253,318,770,L"휴전 27년. 아르켈과 바르카가 함께 사는 국경 마을에 그림자병이 돌아온다.",18);
            r.TextBlock(253,369,770,L"당신은 여울목에서 자란 여행자. 두 목소리를 듣고 숲의 잔해를 잠재워야 한다.",18);
            r.Text(253,421,L"이동 · 대화 · 회피 · 전투 · 정화    약 3~5분",18,Color(0xa8c7bd));
            r.Text(253,502,L"Enter  여정 시작",23,Color(0xf1d29b));
        } else if(finished) {
            r.Text(253,174,L"오늘 밤, 같은 식탁에서",34,Color(0xf1d29b));
            r.Text(253,239,L"여울목의 첫 등불을 지켰습니다.",23);
            const wchar_t* results[]={L"",L"공명기를 중심으로 안정시켰습니다. 사하의 문양이 숲을 보호했습니다.",L"봉인 문양을 중심으로 정화했습니다. 레온의 공명기가 폭주를 막았습니다.",L"공명기와 봉인 문양을 연결했습니다. 두 진영은 공동 관리에 동의했습니다."};
            r.TextBlock(253,292,770,results[choice],18,Color(0xa9cbbb));
            r.TextBlock(253,352,770,L"상처가 사라진 것은 아닙니다. 그래도 서로의 이름을 부르기 시작했습니다.",18);
            r.Text(253,393,L"플레이 시간 "+std::to_wstring(total/60)+L"분 "+std::to_wstring(total%60)+L"초  ·  구조 횟수 "+std::to_wstring(deaths),18);
            r.Text(253,501,L"Enter 마을 둘러보기     R 처음부터 다시",21,Color(0xf1d29b));
        } else if(journal) {
            r.Text(253,170,L"여행 수첩 · 에르마린",30,Color(0xf1d29b));
            r.Text(253,235,L"회복력 27년 — 대전쟁이 휴전으로 끝난 지 스물일곱 해.",19);
            r.TextBlock(253,277,770,L"아르켈 연방  :  질서와 마법 기술. 번영 뒤에는 차별과 전쟁의 책임이 남았다.",17,Color(0x9ec5e0));
            r.TextBlock(253,331,770,L"바르카 연맹  :  자연과 공동체. 깊은 유대 곁에는 불신과 복수심이 남았다.",17,Color(0xe8b28f));
            r.TextBlock(253,385,770,L"세라프의 잔해는 생명을 키웠다. 양쪽의 무분별한 사용은 그림자병을 낳았다.",17);
            r.TextBlock(253,439,770,L"상처를 잊지 않아도 함께 살아갈 수 있는가. 당신의 여정이 답을 찾아간다.",17);
            r.Text(253,502,L"Tab 수첩 닫기",21,Color(0xf1d29b));
        } else {
            r.Text(253,187,L"잠시 쉬어 가기",34,Color(0xf1d29b));
            r.Text(253,269,L"WASD : 화면 기준 8방향 이동     E : 대화·조사",21);
            r.Text(253,320,L"J / 마우스 왼쪽 : 공격     Space : 회피",21);
            r.Text(253,371,L"Tab : 세계관 수첩     정화 선택 : 1 / 2 / 3",21);
            r.Text(253,500,L"Esc 계속하기     R 처음부터 다시",22,Color(0xf1d29b));
        }
    }
}
void TutorialGame::Draw(TutorialRenderer& r) {
    r.Begin(clock); Terrain(r);
    // A shared down-right sun direction. All shadows are laid onto the terrain
    // before depth-sorted objects so they never darken foreground characters.
    for(const auto& prop:props) {
        Vec2 p=Project(prop.p);if(p.x<-230 || p.x>1450 || p.y<-200 || p.y>820)continue;
        if(prop.type==0) {
            Vec2 a=Project(prop.p+Vec2(-prop.w,prop.h));
            Vec2 b=Project(prop.p+Vec2(prop.w,prop.h));
            for(int j=5;j>=1;--j){float f=j*.2f;Vec2 cast(75*f,28*f);
                r.Quad(a,b,b+cast,a+cast,Color(0x101f2c,.035f));}
            r.SoftEllipse(p+Vec2(45,24),105,40,Color(0x122230,.34f));
            r.SoftEllipse(p,85,30,Color(0x101d24,.38f));
        } else if(prop.type==1) {
            r.SoftEllipse(p+Vec2(33,17),58,23,Color(0x122230,.34f));
            r.SoftEllipse(p,14,7,Color(0x101d24,.46f));
        } else r.SoftEllipse(p+Vec2(13,7),25,12,Color(0x13202b,.30f));
    }
    auto shadow=[&](Vec2 world){Vec2 p=Project(world);
        r.SoftEllipse(p+Vec2(14,8),26,10,Color(0x101e2a,.36f));
        r.SoftEllipse(p,15,6,Color(0x101b23,.55f));};
    for(const auto& n:npcs)shadow(n.p);
    if(stage==3)for(const auto& e:enemies)if(e.hp>0)shadow(e.p);
    shadow(player);
    for(const auto& d:dust){float life=d.life/.65f;
        r.SoftEllipse(Project(d.p,3+(1-life)*12),8+(1-life)*13,4+(1-life)*7,Color(0xcdb88d,life*.23f));}
    struct Item { float depth; int kind,index; };
    std::vector<Item> items;
    for(int i=0;i<static_cast<int>(props.size());++i) items.push_back({props[i].p.x+props[i].p.y,0,i});
    for(int i=0;i<static_cast<int>(npcs.size());++i) items.push_back({npcs[i].p.x+npcs[i].p.y,1,i});
    for(int i=0;i<static_cast<int>(enemies.size());++i) if(enemies[i].hp>0 && stage>=3) items.push_back({enemies[i].p.x+enemies[i].p.y,2,i});
    items.push_back({player.x+player.y,3,0});
    std::stable_sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.depth<b.depth;});
    for(const auto& item:items) {
        if(item.kind==0) DrawProp(r,props[item.index]);
        else if(item.kind==1) Person(r,npcs[item.index].p,npcs[item.index].color);
        else if(item.kind==2) {
            const auto& e=enemies[item.index]; Person(r,e.p,e.flash>0?0xe6cddf:0x716080,false,true);
            Vec2 p=Project(e.p,54);r.Rect(p.x-16,p.y,32,4,Color(0x253644));r.Rect(p.x-16,p.y,32*e.hp/40.f,4,Color(0xd58797));
        } else Person(r,player,0x517b7c,true);
    }
    // Floating motes bridge the warm village palette and the cold, corrupted forest.
    for(int i=0;i<120;++i) {
        Vec2 world(float((i*37)%550)/10.f,float((i*17)%290)/10.f);
        Vec2 p=Project(world,20+std::sin(clock*.8f+i)*12);
        p.x+=std::sin(clock*.5f+i)*10;
        r.Ellipse(p,2,2,Color(world.x<forestEdge || restored?0xf2d791:0xa995db,.30f+.2f*std::sin(clock+i)),8);
    }
    for(const auto& n:npcs) {
        Vec2 p=Project(n.p,90);
        if(p.x>0 && p.x<1250 && p.y>140 && p.y<625) {
            r.Text(p.x-20,p.y,n.name,15,Color(0xf0e2c3));
            if(Length(player-n.p)<2.3f) r.Text(p.x-65,p.y-19,n.role,12,Color(0xb9c8c5));
        }
    }
    if(restored) for(int i=0;i<9;++i) {
        Vec2 p=Project({6+i*1.6f,12},75+std::fmod(clock*8+i*19,95.f));
        r.Ellipse(p,16,18,Color(0xffc56f,.08f));r.Rect(p.x-4,p.y-6,8,12,Color(0xeec685,.8f));
    }
    for(int i=0;i<14;++i){Vec2 p=Project({3+i*4.f,31+std::sin(clock*.1f+i)},15);
        r.SoftEllipse(p+Vec2(std::sin(clock*.2f+i)*20,0),110,20,Color(0xc1dbd6,.07f));}
    r.Composite(); Interface(r); r.End();
}
}
