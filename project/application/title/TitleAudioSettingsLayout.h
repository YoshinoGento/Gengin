#pragma once
#include "math/Struct.h"
#include <array>
#include <cmath>
namespace title {
struct AudioSettingsRect {
    Vector2 position, size;
    [[nodiscard]] bool Contains(Vector2 point) const noexcept {
        return std::isfinite(point.x) && std::isfinite(point.y) &&
            point.x>=position.x && point.x<=position.x+size.x &&
            point.y>=position.y && point.y<=position.y+size.y;
    }
};
inline constexpr AudioSettingsRect kAudioSettingsButton{{1068,44},{176,48}};
inline constexpr std::size_t kAudioChannelCount = 3;
inline constexpr std::size_t kContestAudioChannel = 2;
inline constexpr AudioSettingsRect kAudioSettingsPanel{{320,208},{640,440}};
inline constexpr AudioSettingsRect kAudioSettingsReset{{356,572},{176,48}};
inline constexpr AudioSettingsRect kAudioSettingsBack{{780,572},{144,48}};
inline constexpr float kAudioSliderX = 544, kAudioSliderWidth = 340;
inline constexpr std::array<float,kAudioChannelCount> kAudioSliderY{310,398,486};
inline constexpr std::array<AudioSettingsRect,kAudioChannelCount> kAudioSliderHits{{
    {{kAudioSliderX-16,kAudioSliderY[0]-24},{kAudioSliderWidth+32,48}},
    {{kAudioSliderX-16,kAudioSliderY[1]-24},{kAudioSliderWidth+32,48}},
    {{kAudioSliderX-16,kAudioSliderY[2]-24},{kAudioSliderWidth+32,48}}
}};
}
