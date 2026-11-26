#pragma once
namespace fieldsign {
struct Region { float x, y, w, h; };
inline constexpr Region kBoard = {2.0f, 2.0f, 544.0f, 258.0f};
inline constexpr Region kInfo = {552.0f, 2.0f, 264.0f, 60.0f};
inline constexpr Region kStrip = {2.0f, 264.0f, 624.0f, 44.0f};
inline constexpr Region kSlotNormal = {2.0f, 312.0f, 60.0f, 60.0f};
inline constexpr Region kSignNormal = {66.0f, 312.0f, 172.0f, 44.0f};
inline constexpr Region kWideNormal = {242.0f, 312.0f, 456.0f, 48.0f};
inline constexpr Region kSlotFocus = {2.0f, 376.0f, 60.0f, 60.0f};
inline constexpr Region kSignFocus = {66.0f, 376.0f, 172.0f, 44.0f};
inline constexpr Region kWideFocus = {242.0f, 376.0f, 456.0f, 48.0f};
inline constexpr Region kSlotSelected = {2.0f, 440.0f, 60.0f, 60.0f};
inline constexpr Region kSignSelected = {66.0f, 440.0f, 172.0f, 44.0f};
inline constexpr Region kWideSelected = {242.0f, 440.0f, 456.0f, 48.0f};
inline constexpr Region kFill = {995.0f, 495.0f, 4.0f, 4.0f};
}
