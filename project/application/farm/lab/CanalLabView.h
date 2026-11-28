#pragma once
#include "CanalLabControls.h"
#include "2d/Sprite.h"
#include <vector>
namespace canal_lab {
class View {
public:
    bool Initialize(SpriteCommon*);
    void Draw(const System&,const Controls&,float time);
private:
    SpriteCommon* common_=nullptr;
    Sprite background_,bar_,notice_;
    // Each simultaneous draw has its own mapped constants, retired by the frame fence.
    std::vector<Sprite> pieces_;
    size_t used_=0;
    void Part(int col,int row,float x,float y,float w=Layout::W,float h=Layout::H,float alpha=1.f,float angle=0);
};
}
