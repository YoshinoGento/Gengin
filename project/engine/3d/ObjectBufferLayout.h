#pragma once
#include "math/Matrix.h"
#include <d3d12.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace graphics {
struct ObjectMaterial {
    Vector4 color;
    int32_t enableLighting;
    float shininess;
    float environmentCoefficient;
    int32_t specularType;
    Matrix4x4 uvTransform;
    Vector4 surfaceDetail;
    Vector4 outdoorAmbient;
};
struct ObjectTransform {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};
inline constexpr std::size_t kObjectConstantAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
static_assert(sizeof(ObjectMaterial) == 128 && sizeof(ObjectTransform) == 192);
static_assert(offsetof(ObjectMaterial, uvTransform) == 32);
static_assert(offsetof(ObjectMaterial, surfaceDetail) == 96);
static_assert(offsetof(ObjectMaterial, outdoorAmbient) == 112);
// Keep shadow writes separate: main and shadow draws can be in the same GPU submission.
struct ObjectBufferData {
    ObjectMaterial material;
    std::byte materialPadding[kObjectConstantAlignment - sizeof(ObjectMaterial)];
    ObjectTransform transform;
    std::byte transformPadding[kObjectConstantAlignment - sizeof(ObjectTransform)];
    ObjectTransform shadow;
    std::byte shadowPadding[kObjectConstantAlignment - sizeof(ObjectTransform)];
};
static_assert(std::is_standard_layout_v<ObjectBufferData> && std::is_trivially_destructible_v<ObjectBufferData>);
static_assert(offsetof(ObjectBufferData, material) == 0);
static_assert(offsetof(ObjectBufferData, transform) == kObjectConstantAlignment);
static_assert(offsetof(ObjectBufferData, shadow) == 2 * kObjectConstantAlignment);
static_assert(sizeof(ObjectBufferData) == 3 * kObjectConstantAlignment);
}
