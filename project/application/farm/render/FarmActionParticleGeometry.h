#pragma once
#include "farm/render/FarmRainGeometry.h"
#include "farm/system/FarmActionParticles.h"

namespace farm::actionfx {
using rain::Vertex;
// An eight-segment annulus is the most expensive shape in this batch.
inline constexpr std::size_t kPerGrain=8*6, kVertexCount=FarmActionParticles::kCapacity*kPerGrain;
inline std::size_t BuildSownGrain(std::span<Vertex> out,const FieldParticle& p) noexcept {
    constexpr std::size_t sides=6, count=sides*6;
    if (out.size()<count || p.age<0 || p.age>=p.life) return 0;
    const bool seed=p.kind==FieldGrain::Seed;
    const float fade=std::clamp((p.life-p.age)/(seed?.08f:.23f),0.f,1.f);
    const float r=p.radius*(seed?1.f:std::clamp(.32f+p.age*8.f,.32f,1.f));
    const float c=std::cos(p.angle),s=std::sin(p.angle);
    const float pitch=seed?std::clamp((p.position.y-p.floor)/r,0.f,1.f)*.73f*std::sin(p.angle*1.7f):0.f;
    const float cp=std::cos(pitch),sp=std::sin(pitch);
    const auto local=[&](float x,float y,float z) {
        const float u=x*cp-y*sp;
        return p.position+Vector3{r*(c*u-s*z),r*(sp*x+cp*y),r*(s*u+c*z)};
    };
    const Vector3 rim[]{local(1.32f,0,0),local(.32f,.05f,seed?.56f:.83f),
        local(-.63f,0,seed?.62f:.79f),local(-1.04f,0,.05f),
        local(-.57f,.02f,seed?-.60f:-.74f),local(.38f,0,seed?-.43f:-.88f)};
    const auto crown=local(-.16f,seed?.44f:.65f,.05f), bottom=local(0,-.25f,0);
    const Vector3 color=seed?Vector3{.92f,.68f+.12f*p.tone,.36f}:Vector3{.38f+.15f*p.tone,.245f+.09f*p.tone,.14f+.055f*p.tone};
    std::size_t n=0;
    for(std::size_t i=0;i<sides;++i) {
        const auto& a=rim[i]; const auto& b=rim[(i+1)%sides];
        const float facet=static_cast<float>(i);
        const float light=seed?1.16f-.075f*facet:1.20f-.13f*facet;
        const float shade=seed?.65f:.50f;
        const Vector4 top{color.x*light,color.y*light,color.z*light,fade},under{color.x*shade,color.y*shade,color.z*shade,fade};
        out[n++]={crown,top}; out[n++]={a,top}; out[n++]={b,top};
        out[n++]={bottom,under}; out[n++]={b,under}; out[n++]={a,under};
    }
    return n;
}
inline std::size_t Build(std::span<Vertex> out, std::span<const FieldParticle> particles, Vector3 right, Vector3 up) noexcept {
    std::size_t n=0;
    for (const auto& p : particles) {
        if (out.size()-n < kPerGrain) break;
        if (!(p.life>0 && p.age>=0 && p.age<p.life && p.radius>0) || !rain::Finite(p.position)) continue;
        if (p.kind==FieldGrain::Seed || p.kind==FieldGrain::CoverSoil) {
            n+=BuildSownGrain(out.subspan(n),p); continue;
        }
        const float t=p.age/p.life, fade=(std::min)(1.f,(1.f-t)*3.5f);
        const float r=p.radius*(p.kind==FieldGrain::PourDrop && p.bounced?.70f:1.f);
        const auto& c=p.position;
        const auto at=[&](float x,float y,float z){ return Vector3{c.x+x,c.y+y,c.z+z}; };
        const auto tri=[&](Vector3 a,Vector3 b,Vector3 d,Vector4 color){
            out[n++]={a,color}; out[n++]={b,color}; out[n++]={d,color};
        };
        if(p.kind==FieldGrain::Foam) {
            const float a=std::atan2(p.velocity.z,p.velocity.x), x=std::cos(a), z=std::sin(a);
            const auto streak=[&](float along,float across) {
                return at(r*(x*along-z*across),0,r*(z*along+x*across));
            };
            const Vector4 tint{.56f,.83f,.85f,.48f*fade*std::clamp(t*8.f,0.f,1.f)};
            tri(streak(-1.2f,.02f),streak(.1f,-.18f),streak(.92f,-.04f),tint);
            tri(streak(-1.2f,.02f),streak(.92f,-.04f),streak(-.12f,.15f),tint);
        } else if (p.kind==FieldGrain::Ripple || p.kind==FieldGrain::WetSoil || p.kind==FieldGrain::Soak || p.kind==FieldGrain::Seep) {
            const bool soak=p.kind==FieldGrain::Soak || p.kind==FieldGrain::Seep, stain=soak || p.kind==FieldGrain::WetSoil;
            const float radius=r*(soak?.12f+watering::Smooth(t*1.7f):.55f+1.15f*t);
            const float a0=p.angle, sweep=6.2831853f;
            constexpr int segments=8;
            const Vector4 color=soak?Vector4{.12f,.071f,.039f,.75f*fade}:stain?Vector4{.18f,.105f,.058f,.43f*fade}:Vector4{.76f,.96f,1.f,.90f*fade};
            const auto point=[&](int j,float f) {
                const float a=a0+sweep*j/segments;
                return at(radius*f*std::cos(a),.002f,radius*f*std::sin(a)*(stain?.76f:1.f));
            };
            for (int j=0;j<segments;++j) {
                const auto a=point(j,1), b=point(j+1,1);
                if (stain) {
                    auto edge=color; edge.w=0;
                    out[n++]={c,color}; out[n++]={a,edge}; out[n++]={b,edge};
                } else {
                    const auto d=point(j,.69f), e=point(j+1,.69f);
                    tri(a,b,d,color); tri(b,e,d,color);
                }
            }
        } else if (p.kind==FieldGrain::LandingPuff) {
            const float width=r*(.55f+1.6f*t),height=width*(.54f-.18f*t);
            const float dir=p.velocity.x*right.x+p.velocity.z*right.z<0.f?-1.f:1.f;
            const float opacity=(1.f-t)*(1.f-t);
            const auto lobe=[&](Vector3 center,float scale) {
                const auto edge=[&](int j){
                    const float a=p.angle+6.2831853f*j/6.f;
                    return center+right*(std::cos(a)*width*scale)+up*(std::sin(a)*height*scale);
                };
                for(int j=0;j<6;++j) {
                    out[n++]={center,{.84f,.68f,.43f,.88f*opacity}};
                    out[n++]={edge(j),{.72f,.54f,.34f,.19f*opacity}};
                    out[n++]={edge(j+1),{.72f,.54f,.34f,.19f*opacity}};
                }
            };
            lobe(c-right*(dir*width*.76f)+up*(height*.24f),.63f);
            lobe(c,1.f);
        } else if (p.kind==FieldGrain::Dust) {
            const float radius=r*(.65f+1.6f*t);
            const auto rim=[&](int j) {
                const float a=p.angle+6.2831853f*j/6;
                const float u=radius*std::cos(a), v=radius*.76f*std::sin(a);
                return at(right.x*u+up.x*v,right.y*u+up.y*v,right.z*u+up.z*v);
            };
            for (int j=0;j<6;++j) {
                out[n++]={c,{.68f,.49f,.28f,.48f*fade}};
                out[n++]={rim(j),{.72f,.58f,.38f,0}}; out[n++]={rim(j+1),{.72f,.58f,.38f,0}};
            }
        } else {
            const bool drop=p.kind==FieldGrain::Drop || p.kind==FieldGrain::PourDrop, leaf=p.kind==FieldGrain::Leaf;
            const float co=std::cos(p.angle), si=std::sin(p.angle);
            const float roll=p.flutter*std::sin(2.f*p.angle);
            const float cr=p.flutter!=0?std::cos(roll):1.f,sr=p.flutter!=0?std::sin(roll):0.f;
            const auto local=[&](float x,float y,float z){
                const float width=z*cr-y*sr;
                return at(r*(x*co-width*si),r*(y*cr+z*sr),r*(x*si+width*co));
            };
            const float tint=.78f+.22f*p.tone;
            const Vector4 base=leaf?Vector4{.40f,.70f,.12f,fade}:drop?Vector4{.52f,.86f,1.f,.92f*fade}
                :Vector4{.32f*tint,.17f*tint,.075f*tint,fade};
            const Vector3 rim[]{local(leaf?1.45f:1,leaf?.28f:0,0),local(0,leaf?.13f:.17f,leaf?.45f:.76f),
                local(-1,0,0),local(0,-.14f,leaf?-.46f:-.84f)};
            const auto top=local(.07f,leaf?.32f:drop?1.65f:.76f,0), bottom=local(0,leaf?-.08f:drop?-1.12f:-.54f,0);
            for (int j=0;j<4;++j) {
                const float light=1.25f-.14f*j;
                auto face=Vector4{base.x*light,base.y*light,base.z*light,base.w};
                tri(top,rim[j],rim[(j+1)%4],face);
                if (p.flutter!=0) face={.76f*light,.82f*light,.32f*light,base.w};
                else { face.x*=.68f; face.y*=.68f; face.z*=.68f; }
                tri(bottom,rim[(j+1)%4],rim[j],face);
            }
        }
    }
    return n;
}
}
