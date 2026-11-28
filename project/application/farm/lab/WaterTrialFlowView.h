#pragma once
#include "WaterTrialSystem.h"
#include "farm/render/FarmRainGeometry.h"

namespace farm::trial {
enum class Style { Surface, Fall, Spray };
struct SurfaceFrame {
    std::array<Vector3,width*height> centers{};
    float pitch=1, lipY=0, sourceDepth=.12f, openingWidth=.65f;
};
// Samples belong to the view. They never put water back in the grid.
class FlowView final {
    struct Sheet { float age=0, duration=0, rate=0; };
    struct Drop { float age=1, across=0, speed=0; };
    static constexpr float fallTime=.24f, dropLife=.31f;
    std::array<Sheet,20> sheets_{};
    std::array<Drop,10> drops_{};
    size_t cursor_=0;
    uint32_t ordinal_=0;
    float phase_=0, accumulatedImpact_=0;
    float firstTailAge_=1.f, firstWidth_=0;
    bool wasFlowing_=false;
    std::array<Vector4,width*height+1> samples_{};
    std::array<Vector4,width*height> ports_{};
    float rippleAge_=1.f;
    float impactAge_=1, impactRate_=0;
    std::array<rain::Vertex,420> vertices_{};
    size_t n_=0;
    void Tri(Vector3 a,Vector3 b,Vector3 c,Vector4 color) {
        vertices_[n_++]={a,color}; vertices_[n_++]={b,color}; vertices_[n_++]={c,color};
    }
    void Quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Vector4 color) {
        Tri(a,b,c,color); Tri(c,b,d,color);
    }
    void DropFacet(Vector3 a,Vector3 b,Vector3 c) {
        const Vector3 u{c.x-a.x,c.y-a.y,c.z-a.z}, v{b.x-a.x,b.y-a.y,b.z-a.z};
        const Vector3 normal{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const float length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
        const float light=length>1.e-8f?std::clamp((-.4f*normal.x+.8f*normal.y-.44f*normal.z)/length,0.f,1.f):0.f;
        Tri(a,b,c,{.26f+.69f*light,.60f+.38f*light,.68f+.30f*light,.95f});
    }
public:
    FlowView() { samples_.back().w=.84f; }
    std::span<const Vector4> Samples() const { return samples_; }
    std::span<const Vector4> Ports() const { return ports_; }
    float Clock() const { return phase_; }
    void Observe(const System& field) {
        std::array<Vector2,width*height> vectors{};
        const auto flows=field.Flows(); const float dt=field.StepSeconds();
        // Outward-positive rates on W,E,N,S. Shared faces get the same measurement with opposite signs.
        ports_={};
        if(field.Grid().GetTileCount()!=width*height || flows.size()!=ports_.size()) {
            samples_={}; samples_.back().w=.84f; wasFlowing_=false; return;
        }
        if(dt<=0) {
            for(int i=0;i<width*height;++i)
                samples_[i]={0,0,samples_[i].z,field.Grid().GetTile(i)->waterAmount};
            wasFlowing_=false;
            return;
        }
        for(int i=0;i<width*height;++i) {
            const auto& q=flows[i].outward;
            const float east=q[1]-(i==gate?flows[i].sourceRefill:0.f);
            ports_[i]={q[0]/dt,east/dt,q[2]/dt,q[3]/dt};
            vectors[i]={east-q[0],q[3]-q[2]};
        }
        constexpr float cycle=6.2831853f;
        for(int i=0;i<width*height;++i) {
            auto& s=samples_[i]; const auto v=vectors[i];
            const float magnitude=std::hypot(v.x,v.y);
            const float divisor=(std::max)(dt,magnitude);
            s.x=v.x/divisor; s.y=v.y/divisor;
            s.z=std::fmod(s.z+4.5f*(std::min)(magnitude,dt),cycle);
            s.w=field.Grid().GetTile(i)->waterAmount;
        }
        samples_.back()={0,0,.5f*std::sin(phase_*.35f),.84f}; // Pond breeze, not hydraulic flux.
    }
    void Step(float dt,float input) {
        if(!std::isfinite(dt) || dt<=0 || !std::isfinite(input) || input<0) return;
        phase_=std::fmod(phase_+dt,120.f);
        impactAge_+=dt;
        rippleAge_=(std::min)(1.f,rippleAge_+dt);
        firstTailAge_=(std::min)(fallTime,firstTailAge_+dt);
        for(auto& d:drops_) d.age=(std::min)(dropLife,d.age+dt);
        for(auto& s:sheets_) if(s.rate>0) {
            const float before=s.age;
            s.age+=dt;
            if(before<fallTime && s.age>=fallTime) {
                accumulatedImpact_+=s.rate*s.duration; impactAge_=0; impactRate_=s.rate;
            }
            if(s.age>=fallTime+s.duration) s.rate=0;
        }
        if(input>0) {
            if(!wasFlowing_) { firstTailAge_=0; firstWidth_=.075f+.19f*std::clamp(input/dt,0.f,1.f); }
            sheets_[cursor_]={0,dt,input/dt};
            cursor_=(cursor_+1)%sheets_.size();
        }
        wasFlowing_=input>0;
        constexpr float perDrop=.043f;
        if(accumulatedImpact_>=perDrop) {
            accumulatedImpact_=std::fmod(accumulatedImpact_,perDrop);
            auto& d=drops_[ordinal_%drops_.size()];
            rippleAge_=0;
            d={0,rain::Hash(ordinal_+1)-.5f,1.0f+.65f*rain::Hash(ordinal_+43)};
            ++ordinal_;
        }
    }
    std::span<const rain::Vertex> Build(const System& field,const SurfaceFrame& frame,Style style,bool enhanced=false,bool routed=false,bool velocityShape=false) {
        n_=0;
        const auto flows=field.Flows();
        if(!enhanced) for(int i=0;i<width*height;++i) {
            const float volume=flows[i].incoming+flows[i].sourceRefill;
            if(volume<=0 || field.StepSeconds()<=0) continue;
            const auto center=frame.centers[i];
            const auto& q=flows[i].outward;
            const float vx=q[1]-q[0]-flows[i].sourceRefill, vz=q[3]-q[2];
            const float norm=std::hypot(vx,vz);
            if(norm<=0) continue;
            const float dx=vx/norm,dz=vz/norm;
            const float t=std::fmod(phase_*1.5f+i*.17f,1.f)-.5f;
            const float length=frame.pitch*.10f, side=frame.pitch*.023f;
            const auto point=[&](float a,float b) {
                return Vector3{center.x+dx*(t*.48f*frame.pitch+a)-dz*b,center.y+.014f,
                    center.z+dz*(t*.48f*frame.pitch+a)+dx*b};
            };
            const float strength=std::clamp(volume/field.StepSeconds(),0.f,1.f);
            Quad(point(-length,0),point(0,side),point(0,-side),point(length,0),
                {.53f,.85f,.78f,(.18f+.6f*strength)*(1.f-1.7f*std::abs(t))});
        }
        if(style==Style::Surface) return {vertices_.data(),n_};
        const auto gateCenter=frame.centers[gate];
        const auto ribbon=[&](float t,float across) { return Vector3{
            gateCenter.x+frame.pitch*(.5f-.19f*t),std::lerp(frame.lipY,gateCenter.y,t*t)+.014f,
            gateCenter.z+across*frame.pitch}; };
        if(enhanced) {
            constexpr int bands=10;
            const float head=(std::max)(0.f,frame.lipY-gateCenter.y);
            for(int i=0;i<bands && head>.005f;++i) {
                const float a=float(i)/bands,b=float(i+1)/bands,age=(a+b)*.5f*fallTime;
                float rate=0;
                for(const auto& s:sheets_) if(s.rate>0 && s.age<=age && age<s.age+s.duration) rate=s.rate;
                if(age<field.StepSeconds() && wasFlowing_) rate=field.Flows()[gate].sourceRefill/field.StepSeconds();
                if(wasFlowing_ && firstTailAge_<fallTime && age>firstTailAge_) rate=(std::max)(rate,(firstWidth_-.075f)/.19f);
                if(rate<=0) continue;
                const float activity=std::clamp(rate/.5f,0.f,1.f);
                const float w=frame.openingWidth/frame.pitch*.5f*std::sqrt(activity);
                const float thick=(std::min)(head*.3f,frame.sourceDepth)*(.25f+.75f*activity);
                auto face=[&](float t,float side,bool back) {
                    const float edge=velocityShape?1.f+.14f*t*std::sin(phase_*11.f-t*4.f+side):1.f;
                    auto p=ribbon(t,side*w*(1.f-.18f*t)*edge);
                    if(back) {p.x+=thick*t; p.y-=thick*(1.f-t);}
                    return p;
                };
                const float shine=.5f+.5f*std::cos(9*a-phase_*9);
                Quad(face(a,-1,false),face(a,1,false),face(b,-1,false),face(b,1,false),
                    {.12f+.10f*shine,.54f+.16f*shine,.60f+.15f*shine,.77f});
                for(int sign:{-1,1}) Quad(face(a,float(sign),true),face(a,float(sign),false),
                    face(b,float(sign),true),face(b,float(sign),false),{.53f,.87f,.85f,.77f});
            }
            if(impactAge_<.32f && head>.005f) {
                const auto foot=ribbon(1,0);
                const float pulse=routed?std::clamp(rippleAge_/.32f,0.f,1.f):std::fmod(phase_*2.4f,1.f);
                const float radius=frame.openingWidth*(.23f+.40f*pulse)*std::sqrt(std::clamp(impactRate_/.5f,0.f,1.f));
                auto arc=[&](int k,float scale) {float a=-1.50f+k*.25f;
                    return Vector3{foot.x-radius*scale*.57f*std::cos(a),foot.y+.009f,foot.z+radius*scale*std::sin(a)};};
                for(int k=0;k<12;++k) Quad(arc(k,.83f),arc(k,1),arc(k+1,.83f),arc(k+1,1),
                    {.69f,.92f,.84f,.8f*(1-pulse)*(1-impactAge_/.32f)});
            }
        }
        if(!enhanced) for(const auto& s:sheets_) if(s.rate>0) {
            const float top=std::clamp(s.age/fallTime,0.f,1.f);
            const float end=std::clamp((s.age+s.duration)/fallTime,0.f,1.f);
            const float w=.075f+.19f*std::clamp(s.rate,0.f,1.f);
            Quad(ribbon(top,-w),ribbon(top,w),ribbon(end,-w),ribbon(end,w),{.24f,.68f,.68f,.83f});
        }
        // External inflow is already received on this tick. Bridge only the unfilled leading
        // interval; do not make a plot visibly receive from an airborne, disconnected sheet.
        if(!enhanced && firstTailAge_<fallTime) {
            const float t=std::clamp((firstTailAge_+field.StepSeconds())/fallTime,0.f,1.f);
            const float w=firstWidth_;
            Quad(ribbon(t,-w),ribbon(t,w),ribbon(1,-w),ribbon(1,w),{.24f,.68f,.68f,.83f});
        }
        if(style==Style::Spray) for(const auto& d:drops_) if(d.age<dropLife) {
            const float t=d.age/dropLife;
            auto p=ribbon(1,d.across*.30f);
            p.x-=frame.pitch*(routed?.31f:.22f)*t; p.z+=frame.pitch*d.across*(routed?.70f:.55f)*t;
            p.y+=frame.pitch*(routed?.67f:.40f)*d.speed*t*(1.f-t);
            const float r=frame.pitch*(routed?.063f:.050f)*(1.f-.72f*t);
            const float stretch=1.f+.8f*std::abs(1.f-2.f*t);
            Vector3 a{p.x-.48f*r,p.y+r*stretch,p.z+d.across*r}, b{p.x-r,p.y-r*.6f,p.z-r},
                c{p.x+r,p.y-r*.6f,p.z-r}, e{p.x,p.y-r*.6f,p.z+r};
            if(velocityShape && routed) {
                const Vector3 derivative{-.31f,.67f*d.speed*(1-2*t),d.across*.70f};
                const float speed=std::sqrt(derivative.x*derivative.x+derivative.y*derivative.y+derivative.z*derivative.z);
                // Horizontal travel is non-zero, including at the apex. Rotate the entire tetrahedron, not just its tip.
                const float h=std::hypot(derivative.x,derivative.z);
                const Vector3 axis{derivative.x/speed,derivative.y/speed,derivative.z/speed};
                const Vector3 side{-derivative.z/h,0,derivative.x/h};
                const Vector3 up{-axis.y*side.z, h/speed,axis.y*side.x};
                const auto point=[&](float along,float x,float y){return Vector3{
                    p.x+r*(axis.x*along+side.x*x+up.x*y),p.y+r*(axis.y*along+up.y*y),
                    p.z+r*(axis.z*along+side.z*x+up.z*y)};};
                a=point(1.f+.8f*speed,0,0); b=point(-.6f,-.87f,-.5f);
                c=point(-.6f,.87f,-.5f); e=point(-.6f,0,1);
            }
            DropFacet(a,b,c); DropFacet(a,c,e); DropFacet(a,e,b); DropFacet(b,e,c);
        }
        return {vertices_.data(),n_};
    }
};
}
