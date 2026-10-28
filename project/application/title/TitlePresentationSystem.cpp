#include "title/TitlePresentationSystem.h"
#include "title/TitleLogoRipplePoints.h"
#include "title/TitleGrassPresentation.h"
#include "scene/SceneTransitionSystem.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <numbers>

namespace title {
namespace {
constexpr float kQuarterSeconds = kCycleSeconds / 4.0f;
constexpr float kMaximumDelta = 0.25f;
constexpr float kTau = std::numbers::pi_v<float> * 2.0f;
constexpr float kPromptBlinkSeconds = 2.5f;
constexpr float kPromptVisibleSeconds = 1.5f;
constexpr float kPromptFadeSeconds = .2f;
constexpr float kPromptHiddenSeconds = .6f;
static_assert(kPromptVisibleSeconds+kPromptFadeSeconds*2+kPromptHiddenSeconds == kPromptBlinkSeconds);
constexpr Vector2 kOrbitCenter{0.50f, 0.58f};
constexpr Vector2 kOrbitRadius{0.42f, 0.48f};
constexpr float kGrowSeconds = 7.5f;
constexpr float kCropClearSeconds = 9.0f;
constexpr float kTileStagger = 0.18f;
constexpr float kReflectionDayAlpha = .86f;
constexpr float kReflectionNightAlpha = .64f;
constexpr float kReflectionEdgeSeconds = .45f;
static_assert(kReflectionEdgeSeconds*2 < kLogoRippleLifetime);
static_assert(kGrowSeconds < kCropClearSeconds && kCropClearSeconds < kCropCycleSeconds &&
    kCycleSeconds == 4*kCropCycleSeconds);
constexpr std::array<Vector4, 4> kLights{{
    {1.0f, 0.88f, 0.73f, 1.0f}, {0.97f, 1.0f, 0.96f, 1.0f},
    {1.0f, 0.66f, 0.43f, 1.0f}, {0.48f, 0.67f, 0.94f, 1.0f}}};
constexpr std::array<Vector4, 4> kWater{{
    {0.26f, 0.65f, 0.72f, 1.0f}, {0.16f, 0.60f, 0.76f, 1.0f},
    {0.43f, 0.45f, 0.59f, 1.0f}, {0.13f, 0.31f, 0.46f, 1.0f}}};
constexpr std::array<float, 4> kGroundExposure{{.25f, .82f, .25f, .10f}};

float Smooth(float t) noexcept { return t * t * (3.0f - 2.0f * t); }
float Mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
float PromptAlpha(float seconds) noexcept {
    const float phase = std::fmod(seconds,kPromptBlinkSeconds);
    if (phase < kPromptVisibleSeconds) return 1;
    if (phase < kPromptVisibleSeconds+kPromptFadeSeconds)
        return 1-Smooth((phase-kPromptVisibleSeconds)/kPromptFadeSeconds);
    const float fadeInStart = kPromptBlinkSeconds-kPromptFadeSeconds;
    if (phase < fadeInStart) return 0;
    return Smooth((phase-fadeInStart)/kPromptFadeSeconds);
}
Vector4 Mix(Vector4 a, Vector4 b, float t) noexcept {
    return {Mix(a.x,b.x,t), Mix(a.y,b.y,t), Mix(a.z,b.z,t), 1.0f};
}
CelestialBodyFrame MakeCelestialBody(float angle) noexcept {
    CelestialBodyFrame body;
    // Continue through the hidden lower half; opaque terrain, not alpha, causes sunset.
    body.center = {kOrbitCenter.x-kOrbitRadius.x*std::cos(angle),
        kOrbitCenter.y-kOrbitRadius.y*std::sin(angle)};
    return body;
}
}

bool TitlePresentationSystem::Initialize(std::uint32_t windSeed, std::uint32_t rippleSeed) {
    cycleTime_ = entryTime_ = leavingTime_ = 0.0f;
    leaving_ = startConsumed_ = false;
    cloudMotion_.Reset(windSeed);
    grassTime_ = 0;
    ripples_ = {};
    rippleId_ = 0;
    previousRipplePoint_ = kLogoRipplePoints.size();
    if (rippleSeed == 0) {
        const auto ticks = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        rippleSeed = static_cast<std::uint32_t>(ticks ^ (ticks >> 32));
    }
    rippleSeed_ = rippleSeed ? rippleSeed : 1;
    rippleRemaining_ = .9f+.6f*NextRippleValue();
    if (!farm_.Initialize(5, 4)) return false;
    for (int index = 0; index < farm_.GetTileCount(); ++index) {
        const int row = index / farm_.GetWidth();
        const int column = index % farm_.GetWidth();
        farm::FarmTile tile;
        tile.heightLevel = row >= 2 ? row-1 : 0;
        if (column == 2) {
            tile.feature = row == 3 ? farm::FarmTileFeature::WaterSource : farm::FarmTileFeature::Canal;
            tile.waterAmount = 1.0f;
        } else {
            tile.state = farm::FarmTileState::Planted;
            tile.crop = column < 2 ? farm::CropType::Carrot :
                (row >= 2 ? farm::CropType::Tomato : farm::CropType::Pumpkin);
            tile.moisture = 0.65f;
            tile.growth = row == 0 ? 0.55f : 0.95f;
            if (index == 0 || index == 19) {
                tile.state = farm::FarmTileState::Tilled;
                tile.crop = farm::CropType::None;
                tile.growth = 0.0f;
            }
        }
        if (!farm_.SetTile(index, tile)) return false;
    }
    return true;
}

void TitlePresentationSystem::Update(float delta) noexcept {
    if (!std::isfinite(delta) || delta <= 0.0f) return;
    delta = (std::min)(delta, kMaximumDelta);
    UpdateRipples(delta);
    cloudMotion_.Update(delta);
    const float waveSpeed = std::clamp(cloudMotion_.Velocity().x/kGrassWaveWindReference,-1.0f,1.0f);
    grassTime_ = std::fmod(grassTime_+delta*waveSpeed+kGrassFlutterPeriod,kGrassFlutterPeriod);
    cycleTime_ = std::fmod(cycleTime_ + delta, kCycleSeconds);
    entryTime_ = (std::min)(entryTime_ + delta, kFadeSeconds);
    if (leaving_) leavingTime_ = (std::min)(leavingTime_ + delta, scene::kRippleCoverSeconds);
}

float TitlePresentationSystem::NextRippleValue() noexcept {
    rippleSeed_ = rippleSeed_*1664525u+1013904223u;
    return static_cast<float>(rippleSeed_ >> 8)/16777216.0f;
}

void TitlePresentationSystem::UpdateRipples(float delta) noexcept {
    const auto advance = [this](float seconds) {
        for (auto& ripple : ripples_) ripple.age = (std::min)(ripple.age+seconds,kLogoRippleLifetime);
    };
    // Expire slots at the impact time, not at frame end; slot selection stays timestep-independent.
    advance((std::min)(delta,rippleRemaining_));
    rippleRemaining_ -= delta;
    if (rippleRemaining_ > 0) return;
    // Delta is bounded below the minimum interval; lifetime guarantees a free retained slot.
    for (auto& ripple : ripples_) {
        if (ripple.age < kLogoRippleLifetime) continue;
        auto point = static_cast<std::size_t>(NextRippleValue()*kLogoRipplePoints.size());
        point = (std::min)(point,kLogoRipplePoints.size()-1);
        if (point == previousRipplePoint_) point = (point+1)%kLogoRipplePoints.size();
        previousRipplePoint_ = point;
        ripple.center = kLogoRipplePoints[point];
        ripple.age = 0;
        if (++rippleId_ == 0) ++rippleId_;
        ripple.id = rippleId_;
        break;
    }
    advance(-rippleRemaining_);
    const float jitter = (NextRippleValue()+NextRippleValue())*.5f;
    rippleRemaining_ += Mix(kLogoRippleMinimumInterval,kLogoRippleMaximumInterval,jitter);
}

void TitlePresentationSystem::RequestStart() noexcept {
    if (leaving_ || startConsumed_) return;
    leaving_ = true;
    leavingTime_ = 0.0f;
}

bool TitlePresentationSystem::ConsumeStart() noexcept {
    if (!leaving_ || startConsumed_) return false;
    startConsumed_ = true;
    return true;
}

Frame TitlePresentationSystem::GetFrame() const noexcept {
    Frame frame;
    frame.skyFrom = static_cast<std::size_t>(cycleTime_ / kQuarterSeconds) % kLights.size();
    frame.skyTo = (frame.skyFrom + 1) % kLights.size();
    frame.skyBlend = Smooth(std::fmod(cycleTime_, kQuarterSeconds) / kQuarterSeconds);
    const auto from = frame.skyFrom;
    const auto to = frame.skyTo;
    const float blend = frame.skyBlend;
    frame.lightColor = Mix(kLights[from], kLights[to], blend);
    frame.waterColor = Mix(kWater[from], kWater[to], blend);
    frame.groundExposure = Mix(kGroundExposure[from], kGroundExposure[to], blend);
    frame.waterPhase = std::fmod(cycleTime_ / 2.0f, 1.0f);
    frame.windAngle = std::sin(cycleTime_ * kTau / 5.0f) * 0.028f;
    frame.grassWindXZ = GrassWindFromCloudVelocity(cloudMotion_.Velocity());
    frame.grassPhase = grassTime_*kTau/kGrassFlutterPeriod;
    frame.skyOffset = std::sin(cycleTime_ * kTau / kCycleSeconds) * 10.0f;
    constexpr float kDepartureUiSeconds = 0.25f;
    frame.uiAlpha = leaving_ ? 1.0f - Smooth(std::clamp(leavingTime_ / kDepartureUiSeconds, 0.0f, 1.0f)) : 1.0f;
    frame.promptAlpha = PromptAlpha(cycleTime_) * frame.uiAlpha;
    frame.leaving = leaving_;
    frame.entryFadeAlpha = 1.0f - Smooth(entryTime_ / kFadeSeconds);
    frame.fadeAlpha = leaving_ ? Smooth(leavingTime_ / scene::kRippleCoverSeconds) : frame.entryFadeAlpha;
    frame.audio = MakeAudioMix(cycleTime_, frame.fadeAlpha);
    for (std::size_t i = 0; i < frame.crops.size(); ++i) {
        const auto* tile = farm_.GetTile(static_cast<int>(i));
        if (!tile || !farm::IsPlantableCrop(tile->crop)) continue;
        const float age = std::fmod(cycleTime_ + static_cast<float>(i)*kTileStagger, kCropCycleSeconds);
        auto& crop = frame.crops[i];
        crop.visible = age < kCropClearSeconds;
        crop.growth = Mix(.02f, 1.0f, Smooth(std::clamp(age/kGrowSeconds, 0.0f, 1.0f)));
    }
    const float night = (from == 3 ? 1.0f - blend : 0.0f) + (to == 3 ? blend : 0.0f);
    constexpr std::array<Vector4,4> tops{{{.14f,.40f,.61f,1}, {.10f,.43f,.75f,1},
        {.23f,.16f,.36f,1}, {.009f,.022f,.065f,1}}};
    constexpr std::array<Vector4,4> horizons{{{.96f,.62f,.34f,1}, {.60f,.79f,.86f,1},
        {.96f,.39f,.16f,1}, {.05f,.09f,.17f,1}}};
    constexpr std::array<Vector4,4> clouds{{{.96f,.77f,.55f,1}, {.92f,.96f,1,1},
        {.88f,.45f,.31f,1}, {.075f,.13f,.23f,1}}};
    frame.skyTop = Mix(tops[from], tops[to], blend);
    frame.skyHorizon = Mix(horizons[from], horizons[to], blend);
    frame.cloudColor = Mix(clouds[from], clouds[to], blend);
    frame.nightAmount = night;
    for (std::size_t i=0; i<ripples_.size(); ++i) {
        const auto& ripple = ripples_[i];
        if (!ripple.id || ripple.age >= kLogoRippleLifetime) continue;
        auto& output = frame.logoRipples[i];
        output.center = ripple.center;
        output.id = ripple.id;
        const float farX = (std::max)(ripple.center.x,kLogoSize.x-ripple.center.x);
        const float farY = (std::max)(ripple.center.y,kLogoJapaneseHeight-ripple.center.y)*kLogoRippleVerticalScale;
        output.radius = ripple.age/kLogoRippleLifetime*(std::hypot(farX,farY)+
            kLogoRippleEchoDistance+3*kLogoRippleWidth);
        const float entry = Smooth(std::clamp(ripple.age/kReflectionEdgeSeconds,0.0f,1.0f));
        const float exit = Smooth(std::clamp((kLogoRippleLifetime-ripple.age)/kReflectionEdgeSeconds,0.0f,1.0f));
        output.alpha = Mix(kReflectionDayAlpha,kReflectionNightAlpha,night)*entry*exit*frame.uiAlpha;
    }
    const float orbit = cycleTime_ * kTau / kCycleSeconds;
    frame.skyYaw = cloudMotion_.Offset().x;
    frame.cloudLift = cloudMotion_.Offset().y;
    frame.celestialAngle = orbit;
    frame.sun = MakeCelestialBody(orbit);
    frame.moon = MakeCelestialBody(orbit + std::numbers::pi_v<float>);
    for (std::size_t i = 0; i < frame.stars.size(); ++i) {
        auto& star = frame.stars[i];
        star.position = {38.0f + static_cast<float>((i * 197) % 1193),
            22.0f + static_cast<float>((i * 79) % 224)};
        star.size = i % 5 == 0 ? 3.0f : 1.8f;
        star.alpha = night * (0.60f + 0.25f * std::sin(cycleTime_ * kTau / 4.0f + static_cast<float>(i)));
    }
    return frame;
}

} // namespace title
