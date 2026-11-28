#pragma once
#include "BaseScene.h"
#include "farm/lab/CanalLabView.h"
class CanalLabScene final : public BaseScene {
public:
    void Initialize() override;
    InitializationTask InitializeIncrementally() override;
    void Update() override;
    void FixedUpdate(float dt) override;
    void Draw() override;
    void Finalize() override {}
private:
    canal_lab::System field_;
    canal_lab::Controls controls_;
    canal_lab::View view_;
    float phase_=0;
    bool ready_=false;
};
