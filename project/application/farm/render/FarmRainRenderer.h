#pragma once
#include "farm/render/FarmRainGeometry.h"
#include "math/Matrix.h"
#include <d3d12.h>
#include <wrl.h>
class DirectXCommon;
class Camera;

// One transparent batch after opaque geometry, before HUD. Framework owns DirectXCommon.
class FarmRainRenderer final {
public:
    FarmRainRenderer() = default;
    FarmRainRenderer(const FarmRainRenderer&) = delete;
    FarmRainRenderer& operator=(const FarmRainRenderer&) = delete;
    bool Initialize(DirectXCommon* dx);
    void Draw(const farm::FarmRainFrame& frame, const Camera& camera, Vector3 center, float extentX, float extentZ);
private:
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
