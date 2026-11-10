#include "farm/render/FarmRainGeometry.h"
#include <array>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

void Check(bool value, std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("rain presentation line " + std::to_string(at.line()));
}
int main() {
    using Phase=FarmWeatherSystem::Phase;
    farm::FarmRainPresentationSystem rain;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    for (int day : {8,18,28}) {
        rain.Reset();
        for (int i=0;i<100;++i) rain.Update(FarmWeatherSystem::Inspect(day-1,1).phase,true,.05f);
        Check(rain.GetFrame().rain==0 && std::abs(rain.GetFrame().cloud-.25f)<.0001f);
        float last=0;
        for (int i=0;i<120;++i) {
            rain.Update(FarmWeatherSystem::Inspect(day,1).phase,true,.05f);
            const auto frame=rain.GetFrame();
            Check(frame.rain>=last && frame.rain-last<=.05f/2.4f+.0001f);
            Check(frame.LightScale()>=.699f && frame.LightScale()<=1);
            last=frame.rain;
        }
        Check(rain.GetFrame().rain==1 && rain.GetFrame().cloud==1);
        const auto before=rain.GetFrame();
        for (float dt : {0.0f,-1.0f,nan,std::numeric_limits<float>::infinity()}) rain.Update(Phase::Clear,true,dt);
        Check(rain.GetFrame().rain==1 && rain.GetFrame().seconds==before.seconds);
        for (int i=0;i<120;++i) rain.Update(Phase::Clear,true,.05f);
        Check(rain.GetFrame().rain==0 && rain.GetFrame().cloud==0);
    }
    rain.Update(Phase::Rain,true,1000); Check(rain.GetFrame().rain<.05f);
    rain.Update(Phase::Rain,false,.1f); Check(rain.GetFrame().rain==0 && rain.GetFrame().seconds==0);
    rain.Update(Phase::Rain,true,.1f); rain.Update(Phase::Disabled,true,.1f); Check(rain.GetFrame().rain==0);
    for (int day : {8,18,28}) { rain.Update(FarmWeatherSystem::Inspect(day,0).phase,true,.1f); Check(rain.GetFrame().rain==0); }

    std::array<farm::rain::Vertex,farm::rain::kVertexCount+1> vertices{};
    vertices.back().color.w=99;
    farm::FarmRainFrame frame{1,1,10};
    Check(farm::rain::Build(std::span(vertices).first(10),frame,{}, {1,0,0},20,20)==0);
    Check(farm::rain::Build(vertices,frame,{}, {},20,20)==0);
    Check(farm::rain::Build(vertices,frame,{nan,0,0}, {1,0,0},20,20)==0);
    Check(farm::rain::Build(vertices,frame,{}, {1,0,0},nan,20)==0);
    for (int i=0;i<1000;++i) {
        frame.seconds=i*.016;
        const auto count=farm::rain::Build(vertices,frame,{0,.05f,8}, {1,0,0},20,20);
        Check(count==farm::rain::kVertexCount && vertices.back().color.w==99);
        for (std::size_t j=0;j<count;++j) {
            const auto& v=vertices[j]; Check(farm::rain::Finite(v.position));
            Check(v.color.w>=0 && v.color.w<=.42f);
            Check(v.position.y>=-1.95f && v.position.y<10.5f);
        }
    }
    frame.seconds=0; farm::rain::Build(vertices,frame,{}, {1,0,0},20,20);
    const auto head=vertices[2].position;
    frame.seconds=.001; farm::rain::Build(vertices,frame,{}, {1,0,0},20,20);
    Check(vertices[2].position.y<head.y && vertices[2].position.x>head.x);
    frame.seconds=std::numeric_limits<double>::max();
    Check(farm::rain::Build(vertices,frame,{}, {1,0,0},20,20)==farm::rain::kVertexCount);
    for (const auto& v : vertices) Check(farm::rain::Finite(v.position));
    frame.rain=nan; Check(farm::rain::Build(vertices,frame,{}, {1,0,0},20,20)==0);
    frame.rain=1; frame.seconds=std::numeric_limits<double>::infinity();
    Check(farm::rain::Build(vertices,frame,{}, {1,0,0},20,20)==0);
    std::cout << "PASS: rain schedule, fade, clear, disabled, pause clock, finite geometry, direction, capacity and 1000 frames\n";
}
