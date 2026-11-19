#pragma once
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include <cmath>

namespace farm {
// Practice owns its board and command history; no campaign objects are borrowed.
class FarmCanalLessonSystem final {
public:
    enum class Phase { Choose, Preview, Flowing, Reached };
    struct Frame { Phase phase; bool first, rejected, reaches; float before, moisture; };
    static constexpr int kWidth = 5, kHeight = 4, kGap = 12, kFirst = 8, kSecond = 9;
    static constexpr float kDry = 0.08f, kNoticeableGain = 0.04f;

    bool Begin() {
        preview_.Initialize(); water_.Initialize(); tools_.Initialize(); before_ = kDry;
        phase_ = Phase::Choose; target_ = kFirst; rejected_ = false;
        if (!grid_.Initialize(kWidth,kHeight)) return false;
        FarmTile channel{}; channel.feature = FarmTileFeature::WaterSource; channel.waterAmount = 1;
        if (!grid_.SetTile(10,channel)) return false;
        channel.feature = FarmTileFeature::Canal;
        if (!grid_.SetTile(11,channel)) return false;
        channel.waterAmount = 0;
        if (!grid_.SetTile(13,channel)) return false;
        FarmTile plant{}; plant.state = FarmTileState::Planted; plant.crop = CropType::Tomato;
        plant.growth = 0.55f; plant.moisture = kDry;
        if (!grid_.SetTile(kFirst,plant) || !grid_.SetTile(kSecond,plant)) return false;
        water_.Rebuild(grid_);
        return grid_.SetSelectedIndex(kGap);
    }
    bool Preview(int index) {
        if (phase_ != Phase::Choose && phase_ != Phase::Preview) return false;
        Cancel(); rejected_ = true;
        const auto* tile = grid_.GetTile(index);
        if (!tile || tile->feature != FarmTileFeature::None || tile->state != FarmTileState::Empty ||
            (target_ == kFirst && index != kGap)) return false;
        grid_.SetSelectedIndex(index);
        if (!preview_.Begin(grid_, index, FarmIrrigationPreviewOperation::ToggleCanal)) return false;
        phase_ = Phase::Preview; rejected_ = false; return true;
    }
    bool WillReach() const {
        const auto* prediction = preview_.GetPreviewIrrigation();
        return prediction && prediction->IsInIrrigationRange(target_);
    }
    void Cancel() {
        if (phase_ == Phase::Preview) { preview_.Cancel(); phase_ = Phase::Choose; }
        rejected_ = false;
    }
    bool Connect() {
        if (phase_ != Phase::Preview || !WillReach() || !preview_.CanConfirm(grid_) ||
            !tools_.ToggleSelectedCanal(grid_)) return false;
        before_ = Moisture();
        preview_.Cancel(); water_.Rebuild(grid_); grid_.SetSelectedIndex(target_);
        phase_ = Phase::Flowing; return true;
    }
    void Tick(float seconds) {
        if (phase_ != Phase::Flowing || !std::isfinite(seconds) || seconds <= 0) return;
        water_.UpdateWater(grid_,seconds,1.0f);
        if (Moisture() >= before_ + kNoticeableGain) phase_ = Phase::Reached;
    }
    bool TrySecond() {
        if (phase_ != Phase::Reached || target_ != kFirst) return false;
        target_ = kSecond; phase_ = Phase::Choose; grid_.SetSelectedIndex(kSecond);
        rejected_ = false; return true;
    }
    void Move(int dx, int dy) { if (phase_ == Phase::Choose) grid_.MoveSelection(dx,dy); }
    int Selection() const { return grid_.GetSelectedIndex(); }
    int Target() const { return target_; }
    Phase State() const { return phase_; }
    bool Rejected() const { return rejected_; }
    float Moisture() const { const auto* t = grid_.GetTile(target_); return t ? t->moisture : 0.0f; }
    float Before() const { return before_; }
    Frame Read() const { return {phase_,target_ == kFirst,rejected_,WillReach(),before_,Moisture()}; }
    const FarmGrid& Board() const { return grid_; }
    const FarmIrrigationSystem& Water() const { return water_; }
    const FarmIrrigationPreviewSystem& Prediction() const { return preview_; }
private:
    FarmGrid grid_;
    FarmIrrigationSystem water_;
    FarmIrrigationPreviewSystem preview_;
    FarmToolActionSystem tools_;
    Phase phase_ = Phase::Choose;
    int target_ = kFirst;
    float before_ = kDry;
    bool rejected_ = false;
};
}
