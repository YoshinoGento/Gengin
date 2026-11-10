#pragma once
#include "farm/system/FarmRainPresentationSystem.h"
#include "math/Struct.h"
#include <cstdint>
#include <span>

namespace farm::rain {
inline constexpr std::size_t kDropCount = 384;
inline constexpr std::size_t kVertexCount = kDropCount * 6;
struct Vertex final { Vector3 position{}; Vector4 color{}; };
static_assert(sizeof(Vertex) == 28);
inline float Hash(uint32_t value) noexcept {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu; value ^= value >> 16;
    return static_cast<float>(value & 0xffffu) / 65536.0f;
}
inline bool Finite(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
// Fixed world volume; opaque depth naturally hides drops below terrain and behind crops.
inline std::size_t Build(std::span<Vertex> output, const FarmRainFrame& frame,
    Vector3 center, Vector3 cameraRight, float extentX, float extentZ) noexcept {
    if (output.size() < kVertexCount || !Finite(center) || !Finite(cameraRight) ||
        !std::isfinite(frame.rain) || frame.rain <= 0 || !std::isfinite(frame.seconds) || frame.seconds < 0 ||
        !std::isfinite(extentX) || !std::isfinite(extentZ)) return 0;
    const float length = std::sqrt(cameraRight.x*cameraRight.x+cameraRight.y*cameraRight.y+cameraRight.z*cameraRight.z);
    if (!std::isfinite(length) || length < .001f) return 0;
    cameraRight = {cameraRight.x/length,cameraRight.y/length,cameraRight.z/length};
    extentX = std::clamp(extentX,16.0f,40.0f); extentZ = std::clamp(extentZ,16.0f,40.0f);
    constexpr float height = 12.0f, bottom = -2.0f;
    std::size_t count = 0;
    for (uint32_t i=0; i<kDropCount; ++i) {
        const float speed = 7.5f+Hash(i*7+3)*3.0f;
        const double period = static_cast<double>(height) / speed;
        double phase = std::fmod(Hash(i*7+4)-std::fmod(frame.seconds,period)/period,1.0);
        if (phase < 0) phase += 1;
        const float y = static_cast<float>(phase)*height;
        const float streak = .22f+Hash(i*7+5)*.20f;
        const float width = .009f+Hash(i*7+6)*.005f;
        const Vector3 head{center.x+(Hash(i*7+1)-.5f)*extentX-y*.055f,
            center.y+bottom+y,center.z+(Hash(i*7+2)-.5f)*extentZ-y*.018f};
        const Vector3 tail{head.x-streak*.055f,head.y+streak,head.z-streak*.018f};
        const float edge = std::clamp(y/.4f,0.0f,1.0f)*std::clamp((height-y)/.7f,0.0f,1.0f);
        const float alpha = std::clamp(frame.rain,0.0f,1.0f)*edge*.42f;
        const auto point = [&](Vector3 p, float side, float a) {
            return Vertex{{p.x+cameraRight.x*width*side,p.y+cameraRight.y*width*side,p.z+cameraRight.z*width*side},
                {.67f,.78f,.84f,a}};
        };
        const Vertex a=point(tail,-1,0), b=point(tail,1,0), c=point(head,-1,alpha), d=point(head,1,alpha);
        for (const auto& v : {a,b,c,c,b,d}) output[count++]=v;
    }
    return count;
}
}
