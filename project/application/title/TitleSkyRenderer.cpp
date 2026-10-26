#include "title/TitleSkyRenderer.h"
#include "title/TitleCelestialGeometry.h"
#include "3d/Camera.h"
#include "base/DirectXCommon.h"
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include "base/LoadTiming.h"

namespace title {
namespace {
constexpr float kSunHeightFraction = 0.14f;
constexpr float kMoonHeightFraction = 0.085f;
constexpr float kDiscEdgeFraction = 0.06f;
}
bool TitleSkyRenderer::Initialize(DirectXCommon* dx, TextureManager* textures) {
    if (dx_ || !dx || !textures || !textures->IsInitialized() ||
        !std::filesystem::is_regular_file("Resources/title/sky.png")) return false;
    dx_ = dx;
    textures_ = textures;
    texture_ = textures->LoadTexture2D("Resources/title/sky.png");
    if (!texture_.IsValid() || texture_ == textures->GetFallback2D()) return false;
    loading::Scope geometryTiming(loading::Stage::SkyGeometry);
    constexpr UINT longitude = 48, latitude = 24;
    constexpr float pi = 3.14159265359f;
    std::array<Vector4, (longitude+1)*(latitude+1)> vertices{};
    std::array<std::uint32_t, longitude*latitude*6> indices{};
    for (UINT y=0; y<=latitude; ++y) for (UINT x=0; x<=longitude; ++x) {
        const float polar = pi * static_cast<float>(y) / latitude;
        const float azimuth = 2*pi*static_cast<float>(x) / longitude;
        vertices[y*(longitude+1)+x] = {std::sin(polar)*std::cos(azimuth),
            std::cos(polar), std::sin(polar)*std::sin(azimuth), 1};
    }
    std::size_t i = 0;
    for (UINT y=0; y<latitude; ++y) for (UINT x=0; x<longitude; ++x) {
        const UINT a = y*(longitude+1)+x, b = a+longitude+1;
        for (UINT index : {a,b,a+1,a+1,b,b+1}) indices[i++] = index;
    }
    vertices_ = dx_->CreateBufferResource(sizeof(vertices));
    indices_ = dx_->CreateBufferResource(sizeof(indices));
    constants_ = dx_->CreateBufferResource(256);
    if (!vertices_ || !indices_ || !constants_) return false;
    void* memory = nullptr;
    const D3D12_RANGE noRead{0,0};
    if (FAILED(vertices_->Map(0, &noRead, &memory))) return false;
    std::memcpy(memory, vertices.data(), sizeof(vertices)); vertices_->Unmap(0,nullptr);
    if (FAILED(indices_->Map(0, &noRead, &memory))) return false;
    std::memcpy(memory, indices.data(), sizeof(indices)); indices_->Unmap(0,nullptr);
    if (FAILED(constants_->Map(0,&noRead,reinterpret_cast<void**>(&mapped_)))) return false;
    vb_ = {vertices_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(vertices)), sizeof(Vector4)};
    ib_ = {indices_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(indices)), DXGI_FORMAT_R32_UINT};
    indexCount_ = static_cast<UINT>(indices.size());
    geometryTiming.Stop();
    return CreatePipeline();
}

void TitleSkyRenderer::Finalize() noexcept {
    if (constants_ && mapped_) constants_->Unmap(0,nullptr);
    mapped_ = nullptr;
    vertices_.Reset(); indices_.Reset(); constants_.Reset(); root_.Reset(); pipeline_.Reset();
    dx_ = nullptr; textures_ = nullptr; texture_ = {}; vb_ = {}; ib_ = {}; indexCount_ = 0;
}

bool TitleSkyRenderer::CreatePipeline() {
    loading::Scope shaderTiming(loading::Stage::SkyShaders);
    const auto vs = dx_->CompileShader(L"Resources/shader/TitleSky.VS.hlsl", L"vs_6_0");
    const auto ps = dx_->CompileShader(L"Resources/shader/TitleSky.PS.hlsl", L"ps_6_0");
    if (!vs || !ps) return false;
    shaderTiming.Stop();
    loading::Scope rootTiming(loading::Stage::SkyRootSignature);
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].DescriptorTable = {1,&range};
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.pParameters = parameters; signature.NumParameters = 2;
    signature.pStaticSamplers = &sampler; signature.NumStaticSamplers = 1;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error))) return false;
    if (FAILED(dx_->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) return false;
    rootTiming.Stop();
    D3D12_INPUT_ELEMENT_DESC input{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.InputLayout = {&input,1};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    // Draw first, at the far plane, without touching the scene depth buffer.
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.NumRenderTargets = 1;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1; pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    loading::Scope pipelineTiming(loading::Stage::SkyPipeline);
    return SUCCEEDED(dx_->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline_)));
}

void TitleSkyRenderer::Update(const Frame& frame, const Camera& camera, bool reflection) noexcept {
    if (!mapped_) return;
    const auto& projection = camera.GetProjectionMatrix();
    const float horizontalScale = projection.m[0][0], verticalScale = projection.m[1][1];
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
        horizontalScale <= 0 || verticalScale <= 0) return;
    const auto world = MatrixMath::MakeAffineMatrix({60,reflection ? -60.0f : 60.0f,60},{0,0,0},camera.GetTranslate());
    mapped_->wvp = MatrixMath::Multiply(world,camera.GetViewProjectionMatrix());
    mapped_->top = frame.skyTop; mapped_->horizon = frame.skyHorizon; mapped_->clouds = frame.cloudColor;
    const auto& cameraWorld = camera.GetWorldMatrix();
    const auto bodyDirection = [&](const CelestialBodyFrame& body) {
        const auto direction = CelestialDirection(body, projection, cameraWorld);
        return Vector4{direction.x,direction.y,direction.z,1.0f};
    };
    mapped_->sun = bodyDirection(frame.sun);
    mapped_->moon = bodyDirection(frame.moon);
    const float sunRadius = std::atan(kSunHeightFraction/verticalScale);
    const float moonRadius = std::atan(kMoonHeightFraction/verticalScale);
    mapped_->discs = {std::cos(sunRadius),std::cos(sunRadius*(1-kDiscEdgeFraction)),
        std::cos(moonRadius),std::cos(moonRadius*(1-kDiscEdgeFraction))};
    mapped_->motion = {frame.skyYaw, frame.waterPhase, 768, 384};
    mapped_->animation = {frame.cloudLift, frame.celestialAngle,frame.nightAmount,0};
}

void TitleSkyRenderer::Draw() const {
    if (!pipeline_ || !root_ || !mapped_) return;
    auto* commands = dx_->GetCommandList();
    commands->SetPipelineState(pipeline_.Get()); commands->SetGraphicsRootSignature(root_.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0,1,&vb_); commands->IASetIndexBuffer(&ib_);
    commands->SetGraphicsRootConstantBufferView(0,constants_->GetGPUVirtualAddress());
    commands->SetGraphicsRootDescriptorTable(1,textures_->GetGpuHandle(texture_));
    commands->DrawIndexedInstanced(indexCount_,1,0,0,0);
}
}
