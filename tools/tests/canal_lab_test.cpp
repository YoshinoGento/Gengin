#include "farm/lab/CanalLabSystem.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace canal_lab;
void Check(bool pass,const char* why) { if(!pass) throw std::runtime_error(why); }
System Start() {
    std::ifstream map("project/Resources/farm/canal_lab/field.txt"); System s;
    Check(s.Load(map),"map missing or malformed"); return s;
}
void Audit(const System& s) {
    double stock=0;
    for(int y=0;y<Height;++y) for(int x=0;x<Width;++x) {
        auto c=s.At({x,y}); Check(std::isfinite(c->volume)&&c->volume>=0 && c->volume<=2,"invalid volume");
        stock+=c->volume;
    }
    Check(std::abs(s.Budget().admitted-s.Budget().seeped-stock)<.0003,"invented/lost water");
}
int main() try {
    auto dry=Start();Check(dry.Stroke({5,0},{5,3},Tool::Dig),"inland trench");
    for(int i=0;i<600;++i) dry.Step(1.f/60);
    Check(dry.Delivered()==0 && dry.Budget().admitted==0,"dry ditch self-filled");
    auto north=Start(), bank=Start(), west=Start();
    for(auto* s:{&north,&bank}) Check(s->Stroke({5,-1},{5,3},Tool::Dig),"north route");
    Check(bank.Stroke({5,1},{6,1},Tool::Bank),"cast soil");
    Check(west.Stroke({5,3},{-1,3},Tool::Dig),"backwards west route");
    float arrivals[3]{}, full[3]{};
    for(int f=1;f<=2100;++f) {
        int i=0;
        for(auto* s:{&north,&bank,&west}) {
            s->Step(1.f/60);Audit(*s);
            if(!arrivals[i] && s->Delivered()>.03f) arrivals[i]=f/60.f;
            if(!full[i] && s->Delivered()>=System::FullBasin) full[i]=f/60.f;
            ++i;
        }
    }
    for(int i=0;i<3;++i) std::cout<<"route "<<i<<" first="<<arrivals[i]<<" full="<<full[i]<<" s\n";
    std::cout<<"north leak="<<north.Budget().seeped<<", banked="<<bank.Budget().seeped<<", west="<<west.Budget().seeped<<"\n";
    Check(north.Budget().seeped>1 && bank.Budget().seeped==0 && west.Budget().seeped==0,"leak did not depend on terrain/wall");
    Check(full[1]>0 && full[2]>full[1] && (!full[0] || full[0]>full[1]),"routes lack meaningful difference");
    Check(north.Stroke({5,1},{6,1},Tool::Bank),"repair live");
    north.Step(.05f); Check(north.LeakRate()==0,"leak persisted across closed edge");
    Check(north.Stroke({5,1},{4,1},Tool::Bank),"move bank");
    north.Step(.05f);Check(north.LeakRate()>0 && north.At({5,1})->wall==West,"duplicated pile");
    auto other=Start();Check(other.Stroke({11,5},{10,5},Tool::Dig)&&other.Stroke({5,7},{5,6},Tool::Dig),"east and south shore");
    other.Step(.05f);Check(other.At({10,5})->volume>0 && other.At({5,6})->volume>0 && other.Budget().admitted<=.85*.05+.00001,"duplicate pond budget");
    Check(!other.Stroke({2,0},{2,3},Tool::Dig) && !other.At({2,1})->dug && !other.At({2,3})->dug,"cut through rock");
    Check(!other.Stroke({-99,0},{0,0},Tool::Dig) && !other.Stroke({3,3},{4,3},Tool::Bank),"invalid stroke");
    other.Reset(); Audit(other);Check(!other.At({10,5})->dug && other.Budget().admitted==0,"reset retained works");
    std::istringstream bad("...");Check(!other.Load(bad),"short map");
    std::cout<<"PASS: conservation, four shores, containment, live repair, repeat, blocked paths\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
