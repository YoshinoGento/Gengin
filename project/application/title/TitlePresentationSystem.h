#pragma once

#include "farm/core/FarmGrid.h"
#include "title/TitlePresentationLayout.h"
#include "title/TitleAudioMix.h"
#include "title/TitleCloudMotionSystem.h"
#include "math/Struct.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace title {

inline constexpr float kFadeSeconds = 0.8f;
inline constexpr std::size_t kStarCount = 36;
inline constexpr std::size_t kTitleTileCount = 20;
inline constexpr float kCropCycleSeconds = 10.0f;
inline constexpr std::size_t kLogoRippleCount = 2;
inline constexpr float kLogoRippleMinimumInterval = 1.8f;
inline constexpr float kLogoRippleMaximumInterval = 2.8f;
inline constexpr float kLogoRippleLifetime = 3.2f;
inline constexpr float kLogoRippleWidth = 12.0f;
inline constexpr float kLogoRippleEchoDistance = 40.0f;
inline constexpr float kLogoRippleVerticalScale = 1.6f;
inline constexpr float kLogoRippleEchoStrength = .45f;
inline constexpr float kLogoJapaneseHeight = 128.0f;
static_assert(kLogoRippleLifetime < kLogoRippleMinimumInterval*kLogoRippleCount);

struct LogoRippleFrame {
    Vector2 center{};
    float radius = 0.0f;
    float alpha = 0.0f;
    std::uint32_t id = 0;
};

struct CropFrame {
    float growth = 0.0f;
    bool visible = false;
};

struct Star {
    Vector2 position{};
    float alpha = 0.0f;
    float size = 2.0f;
};

struct CelestialBodyFrame {
    Vector2 center{0.08f, 0.58f};
};

struct Frame {
    std::size_t skyFrom = 0;
    std::size_t skyTo = 1;
    float skyBlend = 0.0f;
    float skyOffset = 0.0f;
    float skyYaw = 0.0f;
    float cloudLift = 0.0f;
    float celestialAngle = 0.0f;
    float nightAmount = 0.0f;
    CelestialBodyFrame sun{}, moon{};
    Vector4 skyTop{0.12f, 0.42f, 0.65f, 1.0f};
    Vector4 skyHorizon{0.95f, 0.63f, 0.38f, 1.0f};
    Vector4 cloudColor{0.95f, 0.84f, 0.71f, 1.0f};
    Vector4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
    float groundExposure = .25f;
    Vector4 waterColor{0.2f, 0.6f, 0.8f, 1.0f};
    float waterPhase = 0.0f;
    float windAngle = 0.0f;
    Vector2 grassWindXZ{};
    float grassPhase = 0;
    float promptAlpha = 1.0f;
    float uiAlpha = 1.0f;
    float fadeAlpha = 1.0f;
    float entryFadeAlpha = 1.0f;
    bool leaving = false;
    AudioMix audio{};
    std::array<LogoRippleFrame,kLogoRippleCount> logoRipples{};
    std::array<CropFrame, kTitleTileCount> crops{};
    std::array<Star, kStarCount> stars{};
};

// Staged title data has no document/economy access and never advances a saved farm.
class TitlePresentationSystem final {
public:
    bool Initialize(std::uint32_t windSeed = 0x5A17u, std::uint32_t rippleSeed = 0);
    void Update(float realDeltaSeconds) noexcept;
    // A scene-replacement cover already hides entry; do not run a second black fade.
    void CompleteEntryFade() noexcept { entryTime_ = kFadeSeconds; }
    void RequestStart() noexcept;
    bool ConsumeStart() noexcept;
    [[nodiscard]] Frame GetFrame() const noexcept;
    [[nodiscard]] const farm::FarmGrid& GetFarm() const noexcept { return farm_; }
    [[nodiscard]] bool IsLeaving() const noexcept { return leaving_; }

private:
    farm::FarmGrid farm_;
    float cycleTime_ = 0.0f;
    float entryTime_ = 0.0f;
    float leavingTime_ = 0.0f;
    bool leaving_ = false;
    bool startConsumed_ = false;
    TitleCloudMotionSystem cloudMotion_;
    float grassTime_ = 0;
    struct RippleState {
        Vector2 center{};
        float age = kLogoRippleLifetime;
        std::uint32_t id = 0;
    };
    std::array<RippleState,kLogoRippleCount> ripples_{};
    std::uint32_t rippleSeed_ = 1;
    std::uint32_t rippleId_ = 0;
    std::size_t previousRipplePoint_ = 0;
    float rippleRemaining_ = 0;
    float NextRippleValue() noexcept;
    void UpdateRipples(float delta) noexcept;
};

} // namespace title
