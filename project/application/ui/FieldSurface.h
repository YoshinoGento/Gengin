#pragma once
#include "ui/FieldSurfaceAtlas.h"
#include "math/Struct.h"

namespace fieldsign {
inline constexpr char kTexture[] = "Resources/ui/field_surfaces.png";
inline constexpr Vector4 kPaper{.98f,.96f,.86f,1}, kWheat{1,.84f,.47f,1}, kWater{.58f,.84f,.81f,1};
inline constexpr Vector4 kWhite{1,1,1,1}, kDisabled{.46f,.49f,.46f,1};
[[nodiscard]] constexpr Region Button(float w,float h,bool selected,bool focus) noexcept {
    if (w<=h*1.65f) return selected ? kSlotSelected : focus ? kSlotFocus : kSlotNormal;
    if (w>=h*6) return selected ? kWideSelected : focus ? kWideFocus : kWideNormal;
    return selected ? kSignSelected : focus ? kSignFocus : kSignNormal;
}
[[nodiscard]] constexpr Region Info(float w,float h) noexcept {
    return w>=h*7 ? kStrip : w>=h*3 ? kInfo : kBoard;
}
}
