#pragma once
#include "farm/render/FarmMoldGeometry.h"
#include "math/Matrix.h"
#include <d3d12.h>
#include <wrl.h>
class DirectXCommon;
class Camera;

// One transparent batch after opaque geometry, before HUD. Framework owns DirectXCommon.
class FarmFieldEffectRenderer final {
public:
    FarmFieldEffectRenderer() = default;
    FarmFieldEffectRenderer(const FarmFieldEffectRenderer&) = delete;
    FarmFieldEffectRenderer& operator=(const FarmFieldEffectRenderer&) = delete;
    bool Initialize(DirectXCommon* dx);
    void Draw(const farm::FarmRainFrame& rain, const farm::FarmMoldFrame& mold, const Camera& camera,
        Vector3 center, float extentX, float extentZ);
private:
    static constexpr std::size_t kCapacity = farm::rain::kVertexCount + farm::mold::kVertexCount;
    struct Constants { Matrix4x4 viewProjection; Matrix4x4 world; };
    static_assert(sizeof(Constants)==128); // Shared LineVS b0 layout.
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_, constants_;
    farm::rain::Vertex* mappedVertices_ = nullptr;
    Constants* mappedConstants_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view_{};
};
