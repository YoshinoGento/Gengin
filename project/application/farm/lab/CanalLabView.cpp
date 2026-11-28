#include "CanalLabView.h"
#include <algorithm>
#include <cmath>
namespace canal_lab {
namespace {
void Blit(Sprite& s,float u,float v,float uw,float vh,float x,float y,float w,float h,float alpha=1,float angle=0) {
    s.SetTextureRect({u,v},{uw,vh});s.SetPosition({x,y});s.SetSize({w,h});
    s.SetColor({1,1,1,alpha});s.SetRotation(angle);s.Update();s.Draw();
}
constexpr auto prefix="Resources/farm/canal_lab/";
}
bool View::Initialize(SpriteCommon* common) {
    if(!common || !background_.Initialize(common,std::string(prefix)+"field.png") ||
        !bar_.Initialize(common,std::string(prefix)+"hud.png") || !notice_.Initialize(common,std::string(prefix)+"hud.png")) return false;
    const auto atlas=TextureManager::GetInstance()->LoadTexture2D(std::string(prefix)+"tools.png");
    if(!atlas.IsValid()) return false;
    pieces_.resize(Count*4+3);
    for(auto& s:pieces_) if(!s.Initialize(common,atlas.Index())) return false;
    common_=common;return true;
}
void View::Part(int col,int row,float x,float y,float w,float h,float alpha,float angle) {
    if(used_>=pieces_.size()) return;
    Blit(pieces_[used_++],col*Layout::W,row*Layout::H,Layout::W,Layout::H,x,y,w,h,alpha,angle);
}
void View::Draw(const System& field,const Controls& controls,float time) {
    if(!common_) return;
    common_->PreDraw();used_=0;
    Blit(background_,0,0,1280,720,0,0,1280,720);
    for(int i=0;i<Count;++i) {
        const Point p{i%Width,i/Width};const auto& c=*field.At(p);
        if(!c.dug && c.volume<=.001f) continue;
        const auto [x,y]=Layout::Origin(p);
        const int arms=field.Arms(p);
        if(c.dug) Part(arms,0,x,y);
        if(c.volume>.001f) {
            if(c.dug) {
                int wet=arms;
                for(int d=0;d<4;++d) if(const auto* n=field.At(Next(p,d));n && n->volume<=.001f) wet&=~(1<<d);
                Part(wet,1,x,y,Layout::W,Layout::H,std::clamp(.32f+c.volume*1.5f,.32f,1.f));
            } else {
                const float scale=std::clamp(std::sqrt(c.volume/(c.terrain=='O'?System::FullBasin:.45f)),.08f,1.f);
                Part(11,2,x+(1-scale)*Layout::W*.5f,y+(1-scale)*Layout::H*.5f,Layout::W*scale,Layout::H*scale);
            }
            const float flow=std::hypot(c.currentX,c.currentY);
            if(c.dug && flow>.035f) {
                const float t=std::fmod(time*.75f+i*.37f,1.f)-.5f;
                Part(12,2,x+t*Layout::W*.6f*c.currentX/flow,y+t*Layout::H*.6f*c.currentY/flow,
                    Layout::W,Layout::H,1.f-std::abs(t)*1.65f);
            }
        }
        if(c.dug) Part(c.wall<0?4:c.wall,2,x,y);
    }
    const auto p=controls.Cursor();
    if(Inside(p)||InPondRing(p)) {const auto o=Layout::Origin(p);Part(5,2,o.x,o.y);}
    if(controls.Impact()>0) {
        const float progress=1-controls.Impact()/.36f;
        const auto tip=controls.Tip();
        Part(7+(std::min)(3,int(progress*4)),2,tip.x-42,tip.y-30,84,60,1-progress);
        Part(6,2,tip.x-52,tip.y-18-8*std::sin(progress*3.14f),84,60,1-progress*.7f,(controls.Casting()?-.4f:.3f)*(1-progress));
    }
    Blit(bar_,0,controls.Selected()==Tool::Dig?0.f:80.f,1280,80,0,Layout::Bar,1280,80);
    int status=-1;
    if(field.Delivered()>=System::FullBasin) status=2;
    else if(field.LeakRate()>.04f) status=0;
    else if(field.Delivered()>.03f) status=1;
    if(status>=0) Blit(notice_,status*420.f,160,378,46,451,85,378,46);
}
}
