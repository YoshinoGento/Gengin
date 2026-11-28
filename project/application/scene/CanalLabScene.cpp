#include "CanalLabScene.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include <cmath>
#include <fstream>
void CanalLabScene::Initialize() { (void)InitializeIncrementally().Complete(); }
InitializationTask CanalLabScene::InitializeIncrementally() {
    std::ifstream map("Resources/farm/canal_lab/field.txt");
    ready_=field_.Load(map) && view_.Initialize(Framework::GetInstance()->GetSpriteCommon());
    co_return ready_;
}
void CanalLabScene::Update() {
    auto* app=Framework::GetInstance();
    if(ready_) controls_.Update(field_,*app->GetInput(),*app->GetWinApp(),app->GetFrameClock()->GetRealDeltaSeconds());
}
void CanalLabScene::FixedUpdate(float dt) {
    if(ready_) {field_.Step(dt);phase_=std::fmod(phase_+dt,120.f);}
}
void CanalLabScene::Draw() {if(ready_) view_.Draw(field_,controls_,phase_);}
