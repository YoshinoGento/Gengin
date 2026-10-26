#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include "TransitionSound.h"

namespace scene {
inline constexpr float kDropFallSeconds = .18f;
inline constexpr float kPondDropFallSeconds = .32f;
inline constexpr float kPondSweepDelaySeconds = .12f;
inline constexpr float kRipplePreviewSeconds = .20f;
inline constexpr float kRippleWipeSeconds = .28f;
inline constexpr float kRippleCoverSeconds = kDropFallSeconds+kRipplePreviewSeconds+kRippleWipeSeconds;
inline constexpr float kRippleRevealSeconds = .45f;
inline constexpr float kDropLifetimeSeconds = 2.4f;
inline constexpr std::size_t kMaximumWaterDrops = 4;
inline constexpr float kTransitionMaximumDelta = .25f;
inline constexpr float kWaterClockPeriod = 4096.0f;
inline constexpr float kRippleCenterX = .46f;
inline constexpr float kRippleCenterY = .67f;
inline constexpr float kShopCoverSeconds = .18f;
inline constexpr float kShopRevealSeconds = .26f;
inline constexpr float kShopDistantScale = .945f;
inline constexpr float kFarmApproachScale = 1.045f;

enum class TransitionPhase { Idle, Covering, Revealing, WaitingForRelease };
enum class TransitionTarget { SceneReplacement, ScreenPresentation };
enum class TransitionStyle { Ripple, ShopEnter, ShopExit };
struct RipplePoint { float x = kRippleCenterX, y = kRippleCenterY; };
struct PondRipplePath {
    bool enabled = false;
    RipplePoint start{}, impact{};
    std::array<float,9> screenToPond{};
};
struct WaterDrop { RipplePoint center{}; float age = 0; bool active = false; };
struct TransitionFrame {
    TransitionPhase phase = TransitionPhase::Idle;
    float progress = 0;
    RipplePoint center{};
    std::array<WaterDrop,kMaximumWaterDrops> drops{};
    TransitionStyle style = TransitionStyle::Ripple;
    PondRipplePath pond{};
    float presentationTime = 0;
};
struct TransitionImageMotion { float scale = 1, fade = 0; };

// Transform the complete snapshot so projected 3D signs and 2D labels never drift apart.
inline TransitionImageMotion MakeTransitionImageMotion(const TransitionFrame& frame) noexcept {
    if (frame.style == TransitionStyle::Ripple || !std::isfinite(frame.progress) ||
        (frame.phase != TransitionPhase::Covering && frame.phase != TransitionPhase::Revealing)) return {};
    if (frame.style != TransitionStyle::ShopEnter && frame.style != TransitionStyle::ShopExit) return {};
    const float progress = std::clamp(frame.progress,0.0f,1.0f);
    const bool covering = frame.phase == TransitionPhase::Covering;
    const bool entering = frame.style == TransitionStyle::ShopEnter;
    const float endpoint = covering == entering ? kFarmApproachScale : kShopDistantScale;
    return {covering ? 1+(endpoint-1)*progress : endpoint+(1-endpoint)*progress,
        covering ? progress : 1-progress};
}

// No scene/resource ownership; presentation acknowledgement gates the swap, not elapsed time alone.
class SceneTransitionSystem final {
public:
    bool Begin(RipplePoint point = {}, TransitionTarget target = TransitionTarget::SceneReplacement,
        TransitionSoundTheme sound = TransitionSoundTheme::Water, TransitionStyle style = TransitionStyle::Ripple,
        const PondRipplePath& pond = {}) noexcept {
        if (IsActive() || (style != TransitionStyle::Ripple && style != TransitionStyle::ShopEnter &&
            style != TransitionStyle::ShopExit) ||
            (style != TransitionStyle::Ripple && target != TransitionTarget::ScreenPresentation)) return false;
        if (pond.enabled) {
            if (style != TransitionStyle::Ripple || target != TransitionTarget::SceneReplacement) return false;
            for (float value : pond.screenToPond) if (!std::isfinite(value)) return false;
            for (float value : {pond.start.x,pond.start.y,pond.impact.x,pond.impact.y})
                if (!std::isfinite(value) || value < 0 || value > 1) return false;
        }
        ClearDrops();
        pond_ = pond.enabled ? pond : PondRipplePath{};
        center_ = Sanitize(pond.enabled ? pond.impact : point);
        style_ = style;
        if (style_ == TransitionStyle::Ripple) drops_[0] = {center_,0,true};
        phase_ = TransitionPhase::Covering;
        time_ = 0;
        presentationTime_ = 0;
        coveredPresented_ = false;
        target_ = target;
        screenChangeConsumed_ = false;
        soundTheme_ = sound;
        soundCues_ = 0;
        switch (soundTheme_) {
        case TransitionSoundTheme::SeedShop: Emit(TransitionSoundCue::ShopEnter); break;
        case TransitionSoundTheme::HarvestDisplay: Emit(TransitionSoundCue::DisplayEnter); break;
        case TransitionSoundTheme::ReturnToFarm: Emit(TransitionSoundCue::FarmReturn); break;
        case TransitionSoundTheme::Ledger: Emit(TransitionSoundCue::Ledger); break;
        default: break;
        }
        return true;
    }
    void Update(float delta, bool startInputsReleased) noexcept {
        if (!std::isfinite(delta) || delta < 0) return;
        delta = (std::min)(delta, kTransitionMaximumDelta);
        if (IsCovering() || phase_ == TransitionPhase::Revealing)
            presentationTime_ = std::fmod(presentationTime_+delta,kWaterClockPeriod);
        for (auto& drop : drops_) if (drop.active) {
            drop.age = (std::min)(drop.age+delta,kDropLifetimeSeconds);
            if (drop.age >= kDropLifetimeSeconds) drop.active = false;
        }
        if (phase_ == TransitionPhase::Covering) {
            const float previous = time_;
            time_ = (std::min)(time_+delta,CoverSeconds());
            if (style_ == TransitionStyle::Ripple && soundTheme_ == TransitionSoundTheme::Water) {
                if (previous < FallSeconds() && time_ >= FallSeconds()) Emit(TransitionSoundCue::WaterDrop);
                const float sweepTime = FallSeconds()+(pond_.enabled ? kPondSweepDelaySeconds : kRipplePreviewSeconds);
                if (previous < sweepTime && time_ >= sweepTime) Emit(TransitionSoundCue::WaterSweep);
            }
        }
        else if (phase_ == TransitionPhase::Revealing) {
            time_ = (std::min)(time_+delta,RevealSeconds());
            if (time_ >= RevealSeconds()) phase_ = TransitionPhase::WaitingForRelease;
        } else if (phase_ == TransitionPhase::WaitingForRelease && startInputsReleased) phase_ = TransitionPhase::Idle;
    }
    bool AddDrop(RipplePoint point) noexcept {
        if (IsActive() || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1) return false;
        for (auto& drop : drops_) if (!drop.active) { drop = {point,0,true}; return true; }
        return false; // Keep existing waves intact under rapid repeated clicks.
    }
    void ClearDrops() noexcept { drops_ = {}; }
    void NotifyCoverPresented() noexcept {
        if (phase_ == TransitionPhase::Covering && time_ >= CoverSeconds()) coveredPresented_ = true;
    }
    bool BeginReveal() noexcept {
        if (!CanSwap()) return false;
        phase_ = TransitionPhase::Revealing;
        time_ = 0;
        coveredPresented_ = false;
        ClearDrops();
        if (style_ == TransitionStyle::Ripple && soundTheme_ == TransitionSoundTheme::Water) Emit(TransitionSoundCue::WaterReveal);
        return true;
    }
    [[nodiscard]] TransitionSoundMask ConsumeSoundCues() noexcept {
        const auto cues = soundCues_;
        soundCues_ = 0;
        return cues;
    }
    // Consume before invoking the owner so reentrant requests cannot repeat the screen change.
    bool ConsumeScreenChange() noexcept {
        if (!CanSwap() || target_ != TransitionTarget::ScreenPresentation || screenChangeConsumed_) return false;
        screenChangeConsumed_ = true;
        return true;
    }
    [[nodiscard]] bool IsActive() const noexcept { return phase_ != TransitionPhase::Idle; }
    [[nodiscard]] bool IsCovering() const noexcept { return phase_ == TransitionPhase::Covering; }
    [[nodiscard]] bool CanSwap() const noexcept { return IsCovering() && coveredPresented_; }
    [[nodiscard]] bool BlocksIncomingUpdate() const noexcept { return IsActive() && !IsCovering(); }
    [[nodiscard]] TransitionFrame GetFrame() const noexcept {
        float progress = 0;
        if (IsCovering()) {
            if (style_ != TransitionStyle::Ripple) progress = time_/CoverSeconds();
            else if (pond_.enabled) progress = (time_-FallSeconds())/(kRipplePreviewSeconds+kRippleWipeSeconds);
            else progress = time_ >= CoverSeconds() ? 1.0f :
                (time_-FallSeconds()-kRipplePreviewSeconds)/kRippleWipeSeconds;
        }
        else if (phase_ == TransitionPhase::Revealing) progress = time_/RevealSeconds();
        else if (phase_ == TransitionPhase::WaitingForRelease) progress = 1;
        progress = std::clamp(progress,0.0f,1.0f);
        // Keep the water phase continuous when the old scene is replaced at full cover.
        const float presentationTime = phase_ == TransitionPhase::Idle ? 0 : presentationTime_;
        return {phase_,progress*progress*(3-2*progress),center_,drops_,style_,
            IsActive() ? pond_ : PondRipplePath{},presentationTime};
    }
private:
    [[nodiscard]] float FallSeconds() const noexcept { return pond_.enabled ? kPondDropFallSeconds : kDropFallSeconds; }
    [[nodiscard]] float CoverSeconds() const noexcept { return style_ == TransitionStyle::Ripple ?
        FallSeconds()+kRipplePreviewSeconds+kRippleWipeSeconds : kShopCoverSeconds; }
    [[nodiscard]] float RevealSeconds() const noexcept { return style_ == TransitionStyle::Ripple ? kRippleRevealSeconds : kShopRevealSeconds; }
    void Emit(TransitionSoundCue cue) noexcept { soundCues_ |= SoundBit(cue); }
    static RipplePoint Sanitize(RipplePoint point) noexcept {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return {};
        return {std::clamp(point.x,0.0f,1.0f),std::clamp(point.y,0.0f,1.0f)};
    }
    TransitionPhase phase_ = TransitionPhase::Idle;
    float time_ = 0;
    float presentationTime_ = 0;
    bool coveredPresented_ = false;
    TransitionTarget target_ = TransitionTarget::SceneReplacement;
    TransitionStyle style_ = TransitionStyle::Ripple;
    bool screenChangeConsumed_ = false;
    TransitionSoundTheme soundTheme_ = TransitionSoundTheme::Water;
    TransitionSoundMask soundCues_ = 0;
    RipplePoint center_{};
    PondRipplePath pond_{};
    std::array<WaterDrop,kMaximumWaterDrops> drops_{};
};
}
