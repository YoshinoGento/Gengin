#pragma once
#include "BaseScene.h"
#include "farm/lab/WaterTrialFlowView.h"
#include "farm/lab/TrialWaterProbe.h"
#include "farm/render/FarmFieldEffectRenderer.h"
#include "farm/render/FarmRenderer.h"
#include "3d/Camera.h"
#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

class WaterTrialScene final : public BaseScene {
public:
    void Initialize() override;
    InitializationTask InitializeIncrementally() override;
    void FixedUpdate(float dt) override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
private:
    bool Reset(bool both);
    void Record(const char* event);
    void Fit(float aspect);
    void PrepareView();
    void PrepareInspector();
    farm::trial::System field_;
    farm::trial::FlowView flowView_;
    farm::trial::SurfaceFrame surface_{};
    farm::FarmVisualSystem visual_;
    farm::FarmRenderer renderer_;
    FarmFieldEffectRenderer effects_;
    Camera camera_;
    BitmapFont font_;
    std::array<SpriteText,7> labels_;
    std::array<Sprite,4> tabs_;
    std::array<SpriteText,4> tabLabels_;
    std::array<SpriteText,6> inspectionLines_;
    std::array<std::array<int,5>,2> shown_{};
    bool inspecting_=false;
    farm::trial::Style style_=farm::trial::Style::Surface;
    farm::trial::Work tool_=farm::trial::Work::Dig;
    long long run_=0;
    int hover_=-1;
    float aspect_=0;
    bool ready_=false, paused_=false, logOk_=true;
    bool reference_=false;
    bool enhanced_=true, routed_=true, localIntake_=true;
    farm::trial::SoilPolicy soil_=farm::trial::SoilPolicy::HarvestCost;
    farm::CanalTransport transport_=farm::CanalTransport::LocalEdges;
    bool velocityShape_=true;
    int replayTick_=-1;
    int captureLeadIn_=0;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> waterPass_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> routedPass_;
    farm::trial::WaterProbe probe_;
};
