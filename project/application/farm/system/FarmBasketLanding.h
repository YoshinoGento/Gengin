#pragma once
#include "math/Struct.h"
#include "farm/system/FarmCropSizeSystem.h"

namespace farm::harvest {
enum class SizeCue { Small, Regular, Large };
inline SizeCue CueFor(const FarmCropSizeResult& size) noexcept {
    constexpr float smallLimit=.85f,largeThreshold=1.5f;
    if(!size.known || !size.IsConsistent()) return SizeCue::Regular;
    return size.multiplier<smallLimit?SizeCue::Small:size.multiplier>=largeThreshold?SizeCue::Large:SizeCue::Regular;
}
template<class T> constexpr T BySize(SizeCue size,T lightValue,T normalValue,T heavyValue) noexcept {
    return size==SizeCue::Small?lightValue:size==SizeCue::Large?heavyValue:normalValue;
}
// A render-time contact, never a second harvest or an inventory operation.
struct BasketLanding {
    Vector3 rim{};
    float floor{}, side{}, unit{}, moisture{};
    SizeCue size=SizeCue::Regular;
    Vector3 crateCenter{};
    Vector2 footprintHalf{};
};
}
