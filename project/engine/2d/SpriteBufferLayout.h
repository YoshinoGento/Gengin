#pragma once
#include "math/Matrix.h"
#include <d3d12.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace graphics {
struct SpriteVertex { Vector4 position; Vector4 texcoord; };
struct SpriteMaterial {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};
struct SpriteTransform { Matrix4x4 WVP; Matrix4x4 World; };
inline constexpr std::size_t kSpriteConstantAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
static_assert(sizeof(SpriteVertex) * 4 + sizeof(uint32_t) * 6 < kSpriteConstantAlignment);
static_assert(sizeof(SpriteMaterial) < kSpriteConstantAlignment && sizeof(SpriteTransform) < kSpriteConstantAlignment);

// One owned upload allocation per Sprite. CBV addresses must remain 256-byte aligned.
struct SpriteBufferData {
    SpriteVertex vertices[4];
    uint32_t indices[6];
    std::byte geometryPadding[kSpriteConstantAlignment - sizeof(vertices) - sizeof(indices)];
    SpriteMaterial material;
    std::byte materialPadding[kSpriteConstantAlignment - sizeof(SpriteMaterial)];
    SpriteTransform transform;
    std::byte transformPadding[kSpriteConstantAlignment - sizeof(SpriteTransform)];
};
static_assert(std::is_standard_layout_v<SpriteBufferData>);
static_assert(std::is_trivially_destructible_v<SpriteBufferData>);
static_assert(offsetof(SpriteBufferData, material) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
static_assert(offsetof(SpriteBufferData, transform) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
static_assert(sizeof(SpriteMaterial) == 96 && sizeof(SpriteTransform) == 128);
static_assert(sizeof(SpriteBufferData) == 768);
}
