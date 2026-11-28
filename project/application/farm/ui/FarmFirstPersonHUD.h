#pragma once
#include "2d/Sprite.h"
#include <array>
#include <cmath>

namespace farmui {
struct FirstPersonFrame {
    int slot{}, message{};
    bool valid{}, reached{}, gripping{};
    float swing{};
};
class FirstPersonHUD {
public:
    bool Initialize(SpriteCommon* common) {
        for(auto& s:parts_) if(!s.Initialize(common,"Resources/farm/first_person/hud.png")) return false;
        return true;
    }
    void Draw(const FirstPersonFrame& f) {
        const auto blit=[&](size_t i,float u,float v,float w,float h,float x,float y,float scale=1.f) {
            auto& s=parts_[i]; s.SetTextureRect({u,v},{w,h}); s.SetPosition({x,y});
            s.SetSize({w*scale,h*scale}); s.Update(); s.Draw();
        };
        constexpr std::array<int,6> toolArt{0,1,2,2,2,3};
        const float swing=std::sin(f.swing*3.14159f);
        if (!f.gripping) blit(0,256.f*toolArt.at(f.slot),0,256,256,1022-62*swing,473+35*swing,1.03f);
        blit(1,0,256.f+f.slot*48,240,48,24,646);
        blit(2,0,740,686,34,24,24);
        blit(3,730.f+(f.valid?2:f.reached?1:0)*40.f,740,32,32,624,344);
        blit(4,260,256.f+f.message*32,360,32,24,607);
    }
private:
    std::array<Sprite,5> parts_{};
};
}
