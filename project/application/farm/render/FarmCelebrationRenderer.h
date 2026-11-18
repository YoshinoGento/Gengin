#pragma once
#include "farm/render/FarmCelebrationGeometry.h"
#include "math/Matrix.h"
#include <d3d12.h>
#include <wrl.h>
class DirectXCommon;
class Camera;

// One transparent batch after opaque geometry, before HUD. Framework owns DirectXCommon.
class FarmCelebrationRenderer final {
public:
    FarmCelebrationRenderer() = default;
    FarmCelebrationRenderer(const FarmCelebrationRenderer&) = delete;
    FarmCelebrationRenderer& operator=(const FarmCelebrationRenderer&) = delete;
    bool Initialize(DirectXCommon* dx);
    void Draw(bool active, float seconds, const Camera& camera);
private:
    struct Constants { Matrix4x4 viewProjection; Matrix4x4 world; };
    static_assert(sizeof(Constants)==128); // Shared LineVS b0 layout.
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_, constants_;
    farm::celebration::Vertex* mappedVertices_ = nullptr;
    Constants* mappedConstants_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view_{};
};
