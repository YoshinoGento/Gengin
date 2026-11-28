#include "CanalLabSystem.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace canal_lab {
namespace {
bool Wettable(const Cell& c) { return c.dug || c.terrain=='O' || c.terrain=='~'; }
float Capacity(const Cell& c) { return c.terrain=='O'?2.f:1.f; }
float Head(const Cell& c) {
    const float bed=c.terrain=='~'?-.58f:c.terrain=='O'?-.25f:0.f;
    return bed+.7f*c.volume/Capacity(c);
}
}
bool System::Load(std::istream& map) {
    System next;
    std::string line;
    for(int y=0;y<Height;++y) {
        if(!std::getline(map,line)) return false;
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.size()!=Width) return false;
        for(int x=0;x<Width;++x) {
            const char ch=line[x];
            if(ch!='.' && ch!='~' && ch!='O' && ch!='#') return false;
            const int i=Index({x,y});
            next.cells_[i].terrain=ch;
            if(ch=='O') { if(next.basin_>=0) return false; next.basin_=i; }
        }
    }
    if(next.basin_<0 || std::getline(map,line)) return false;
    *this=next; return true;
}
void System::Reset() noexcept {
    for(auto& c:cells_) c=Cell{c.terrain};
    balance_={}; leakRate_=0;
}
bool System::Dig(Point p) noexcept {
    if(!Inside(p)) return InPondRing(p);
    auto& c=cells_[Index(p)];
    if(c.terrain=='O') return true;
    if(c.terrain!='.') return false;
    c.dug=true; return true;
}
bool System::Edge(Point a,Point b,int d,Tool tool) noexcept {
    if(tool==Tool::Bank) {
        if(!Inside(a) || !cells_[Index(a)].dug) return false;
        // A tile yields exactly one pile; a new cast moves that pile rather than duplicating it.
        cells_[Index(a)].wall=d; return true;
    }
    if(!Dig(a) || !Dig(b)) return false;
    const auto open=[&](Point p,int side) {
        auto& c=cells_[Index(p)];
        if(c.dug) c.mouths|=1u<<side;
    };
    if(Inside(a) && !Inside(b)) open(a,d);
    if(!Inside(a) && Inside(b)) open(b,Opposite(d));
    return true;
}
bool System::Stroke(Point a,Point b,Tool tool) noexcept {
    if((!Inside(a) && !InPondRing(a)) || (!Inside(b) && !InPondRing(b)) || a==b) return false;
    const int nx=std::abs(b.x-a.x),ny=std::abs(b.y-a.y);
    int ix=0,iy=0; bool changed=false;
    while(ix<nx || iy<ny) {
        const bool horizontal=ix<nx && (iy==ny || (2*ix+1)*ny<=(2*iy+1)*nx);
        const int d=horizontal?(b.x>a.x?East:West):(b.y>a.y?South:North);
        const auto to=Next(a,d);
        if(!Edge(a,to,d,tool)) return false;
        changed=true;
        if(tool==Tool::Bank) break;
        a=to; if(horizontal) ++ix; else ++iy;
    }
    return changed;
}
bool System::Linked(Point a,int d) const noexcept {
    const auto& c=cells_[Index(a)];
    if(!Wettable(c) || c.wall==d) return false;
    const auto* n=At(Next(a,d));
    return n ? Wettable(*n) && n->wall!=Opposite(d) : (c.mouths&(1u<<d))!=0;
}
int System::Arms(Point p) const noexcept {
    int mask=0;
    if(Inside(p)) for(int d=0;d<4;++d) if(Linked(p,d)) mask|=1<<d;
    return mask;
}
void System::Step(float seconds) noexcept {
    if(basin_<0 || !std::isfinite(seconds) || seconds<=0) return;
    const float t=std::min(seconds,.1f);
    const int steps=static_cast<int>(std::ceil(t*60.f));
    for(int i=0;i<steps;++i) Flow(t/steps);
}
void System::Flow(float dt) noexcept {
    constexpr float supplyRate=.85f, conductance=5.5f, seepRate=.38f;
    std::array<float,Count> delta{},intake{};
    leakRate_=0;
    for(auto& c:cells_) c.currentX=c.currentY=0;
    int mouths=0;
    for(int i=0;i<Count;++i) if(cells_[i].mouths) {
        const auto& c=cells_[i];
        for(int d=0;d<4;++d) if((c.mouths&(1u<<d)) && c.wall!=d) { ++mouths; ++intake[i]; }
    }
    // All edges read the same water levels. Each edge may spend at most one quarter
    // of either tile's stock or free space, including an edge facing the outside pond.
    for(int i=0;i<Count;++i) {
        const auto& c=cells_[i]; if(!Wettable(c)) continue;
        const Point p{i%Width,i/Width};
        if(intake[i]>0) {
            const float q=std::min(supplyRate*dt*intake[i]/mouths,(Capacity(c)-c.volume)*.24f*intake[i]);
            delta[i]+=q; balance_.admitted+=q;
            for(int d=0;d<4;++d) if((c.mouths&(1u<<d)) && c.wall!=d) {
                cells_[i].currentX-=Offset[d].x*q/intake[i]/dt;
                cells_[i].currentY-=Offset[d].y*q/intake[i]/dt;
            }
        }
        for(int d:{East,South}) {
            const Point n=Next(p,d);
            if(!Inside(n) || !Linked(p,d)) continue;
            int src=i,dst=Index(n);
            float difference=Head(c)-Head(cells_[dst]);
            if(difference<0) {std::swap(src,dst);difference=-difference;}
            const float q=std::min({difference*conductance*dt,cells_[src].volume*.24f,
                (Capacity(cells_[dst])-cells_[dst].volume)*.24f});
            delta[src]-=q;delta[dst]+=q;
            const float velocity=(src==i?q:-q)/dt;
            for(int end:{src,dst}) {
                cells_[end].currentX+=velocity*Offset[d].x;
                cells_[end].currentY+=velocity*Offset[d].y;
            }
            if(cells_[src].dug && cells_[dst].terrain=='~') leakRate_+=q/dt;
        }
    }
    for(int i=0;i<Count;++i) {
        auto& c=cells_[i]; c.volume+=delta[i];
        if(c.terrain=='~') {
            const float loss=std::min(c.volume*.15f,seepRate*dt);
            c.volume-=loss; balance_.seeped+=loss;
        }
    }
}
}
