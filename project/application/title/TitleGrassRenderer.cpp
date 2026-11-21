#include "title/TitleGrassRenderer.h"
#include "title/TitleGrassPresentation.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "base/SrvManager.h"
#include "io/JsonFile.h"
#include <array>
#include <cstring>
#include <filesystem>

namespace title {
namespace {
bool ReadFloats(const nlohmann::json& object, const char* name, float* output, std::size_t count) {
    const auto found = object.find(name);
    if (found == object.end() || !found->is_array() || found->size() != count) return false;
    for (std::size_t i=0; i<count; ++i) {
        if (!(*found)[i].is_number()) return false;
        const double value = (*found)[i].get<double>();
        if (!std::isfinite(value) || std::abs(value) > 1000) return false;
        output[i] = static_cast<float>(value);
    }
    return true;
}
}
bool TitleGrassRenderer::Initialize(Object3dCommon* common, ModelManager* models, const char* layoutPath) {
    if (common_ || !common || !models || !common->GetDxCommon() || !common->GetLightingSystem() ||
        !common->GetSrvManager() || !common->IsShadowReady() ||
        !std::filesystem::is_regular_file("Resources/title/grass_patch.obj") ||
        !std::filesystem::is_regular_file("Resources/title/grass_far_patch.obj")) return false;
    nlohmann::json layout;
    if (!layoutPath || !JsonFile::Load(layoutPath,layout) || !layout.is_object() ||
        !layout.contains("version") || layout["version"] != 1 || !layout.contains("patches") ||
        !layout["patches"].is_array() || layout["patches"].empty() || layout["patches"].size() > kMaximumPatches ||
        !layout.contains("shadowCount") || !layout["shadowCount"].is_number_unsigned() ||
        layout["shadowCount"].get<std::uint64_t>() > layout["patches"].size()) return false;
    std::array<Instance,kMaximumPatches> instances{};
    UINT count = 0;
    for (const auto& patch : layout["patches"]) {
        if (!patch.is_object()) return false;
        std::array<float,3> position{};
        std::array<float,4> tint{};
        if (!ReadFloats(patch,"position",position.data(),position.size()) ||
            !ReadFloats(patch,"tint",tint.data(),tint.size()) || !patch.contains("scale") ||
            !patch["scale"].is_number() || !patch.contains("yaw") || !patch["yaw"].is_number()) return false;
        const double scale = patch["scale"].get<double>(), yaw = patch["yaw"].get<double>();
        if (!std::isfinite(scale) || scale < .75 || scale > 1.2 || !std::isfinite(yaw) || std::abs(yaw) > 7 ||
            std::abs(position[0]) > 50 || position[2] < -74 || position[2] > 86 ||
            position[1] < -.3f || position[1] > 2) return false;
        for (float channel : tint) if (channel < 0 || channel > 1) return false;
        if (tint[3] != 1) return false;
        instances[count++] = {{position[0],position[1],position[2],static_cast<float>(scale)},
            {tint[0],tint[1],tint[2],tint[3]},
            {static_cast<float>(std::cos(yaw)),static_cast<float>(std::sin(yaw))},{}};
    }
    models->LoadModel("title/grass_patch.obj");
    models->LoadModel("title/grass_far_patch.obj");
    model_ = models->GetModel("title/grass_patch.obj");
    farModel_ = models->GetModel("title/grass_far_patch.obj");
    if (!model_ || !farModel_ || model_->HasSkinCluster() || farModel_->HasSkinCluster() ||
        !model_->GetIndexCount() || !farModel_->GetIndexCount()) return false;
    common_ = common;
    auto* dx = common_->GetDxCommon();
    instances_ = dx->CreateBufferResource(sizeof(Instance)*count);
    mainConstants_ = dx->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    shadowConstants_ = dx->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    reflectionConstants_ = dx->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (!instances_ || !mainConstants_ || !shadowConstants_ || !reflectionConstants_) { Finalize(); return false; }
    const D3D12_RANGE noRead{0,0};
    void* mapped = nullptr;
    if (FAILED(instances_->Map(0,&noRead,&mapped))) { Finalize(); return false; }
    std::memcpy(mapped,instances.data(),sizeof(Instance)*count);
    instances_->Unmap(0,nullptr);
    if (FAILED(mainConstants_->Map(0,&noRead,reinterpret_cast<void**>(&mainMapped_))) ||
        FAILED(shadowConstants_->Map(0,&noRead,reinterpret_cast<void**>(&shadowMapped_))) ||
        FAILED(reflectionConstants_->Map(0,&noRead,reinterpret_cast<void**>(&reflectionMapped_)))) { Finalize(); return false; }
    *mainMapped_ = {}; *shadowMapped_ = {};
    instancesView_ = {instances_->GetGPUVirtualAddress(),static_cast<UINT>(sizeof(Instance)*count),sizeof(Instance)};
    count_ = count;
    shadowCount_ = layout["shadowCount"].get<UINT>();
    ready_ = CreatePipelines();
    if (!ready_) Finalize();
    return ready_;
}

bool TitleGrassRenderer::CreatePipelines() {
    auto* dx = common_->GetDxCommon();
    const auto vs = dx->CompileShader(L"Resources/shader/TitleGrass.VS.hlsl",L"vs_6_0");
    const auto ps = dx->CompileShader(L"Resources/shader/TitleGrass.PS.hlsl",L"ps_6_0");
    const auto reflectedPS = dx->CompileShader(L"Resources/shader/TitleGrassReflection.PS.hlsl",L"ps_6_0");
    if (!vs || !ps || !reflectedPS) return false;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
    std::array<D3D12_ROOT_PARAMETER,3> parameters{};
    parameters[0].ParameterType = parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].Descriptor.ShaderRegister = 1;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable = {1,&range};
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.MaxAnisotropy = 1;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.NumParameters = static_cast<UINT>(parameters.size()); signature.pParameters = parameters.data();
    signature.NumStaticSamplers = 1; signature.pStaticSamplers = &sampler;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error)) ||
        FAILED(dx->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) return false;
    const std::array<D3D12_INPUT_ELEMENT_DESC,6> inputs{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32_FLOAT,1,32,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1}
    }};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get(); pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.InputLayout = {inputs.data(),static_cast<UINT>(inputs.size())};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.NumRenderTargets = 2;
    pso.RTVFormats[0] = pso.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1; pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    if (FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&mainPipeline_)))) return false;
    pso.PS = {reflectedPS->GetBufferPointer(),reflectedPS->GetBufferSize()};
    pso.NumRenderTargets = 1; pso.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
    if (FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&reflectionPipeline_)))) return false;
    pso.PS = {}; pso.NumRenderTargets = 0; pso.RTVFormats[0] = pso.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.RasterizerState.DepthBias = 1200; pso.RasterizerState.SlopeScaledDepthBias = 1.5f;
    return SUCCEEDED(dx->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&shadowPipeline_)));
}

void TitleGrassRenderer::Update(const Frame& frame, const Camera& camera) noexcept {
    if (!ready_) return;
    const auto& light = common_->GetLightingSystem()->GetDirectionalLight();
    mainMapped_->viewProjection = camera.GetViewProjectionMatrix();
    mainMapped_->lightColor = light.color;
    // Horizontal-ground compensation is not valid for two-sided leaves at sunrise.
    const float exposure = GrassExposure(frame.groundExposure);
    mainMapped_->lightDirectionIntensity = {light.direction.x,light.direction.y,light.direction.z,exposure};
    mainMapped_->wind = {frame.grassWindXZ.x,frame.grassWindXZ.y,frame.grassPhase,1/kGrassBendHeight};
    const auto windAxis = vegetation::WaveAxis();
    mainMapped_->wave = {windAxis.x,windAxis.y,
        kGrassWaveSpatialFrequency,kGrassTuftPhaseSpread};
}
void TitleGrassRenderer::BindAndDraw(bool shadow) {
    auto* commands = common_->GetDxCommon()->GetCommandList();
    commands->SetGraphicsRootSignature(root_.Get());
    commands->SetPipelineState(shadow ? shadowPipeline_.Get() : mainPipeline_.Get());
    commands->SetGraphicsRootConstantBufferView(0,(shadow ? shadowConstants_ : mainConstants_)->GetGPUVirtualAddress());
    if (!shadow) {
        common_->GetSrvManager()->PreDraw();
        commands->SetGraphicsRootConstantBufferView(1,common_->GetShadowSceneBufferAddress());
        commands->SetGraphicsRootDescriptorTable(2,common_->GetShadowMapSrvHandle());
    }
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(1,1,&instancesView_);
    // Static Model binds slot0 only; slot1 remains immutable per-instance data.
    if (shadowCount_) model_->Draw(commands,shadowCount_);
    if (!shadow && count_ > shadowCount_) {
        auto farView = instancesView_;
        const UINT offset = shadowCount_*sizeof(Instance);
        farView.BufferLocation += offset;
        farView.SizeInBytes -= offset;
        commands->IASetVertexBuffers(1,1,&farView);
        farModel_->Draw(commands,count_-shadowCount_);
    }
}
void TitleGrassRenderer::DrawShadow() {
    if (!ready_ || !shadowCount_ || !common_->IsShadowReady()) return;
    *shadowMapped_ = *mainMapped_;
    shadowMapped_->viewProjection = common_->GetLightViewProjectionMatrix();
    BindAndDraw(true);
}
void TitleGrassRenderer::Draw() {
    if (ready_ && common_->IsObjectPassActive()) BindAndDraw(false);
}
void TitleGrassRenderer::DrawReflection(float waterHeight) {
    if (!ready_ || !std::isfinite(waterHeight)) return;
    *reflectionMapped_ = *mainMapped_;
    auto mirror = MatrixMath::MakeIdentity4x4();
    mirror.m[1][1] = -1; mirror.m[3][1] = 2*waterHeight;
    reflectionMapped_->viewProjection = MatrixMath::Multiply(mirror,mainMapped_->viewProjection);
    auto* commands = common_->GetDxCommon()->GetCommandList();
    commands->SetGraphicsRootSignature(root_.Get()); commands->SetPipelineState(reflectionPipeline_.Get());
    commands->SetGraphicsRootConstantBufferView(0,reflectionConstants_->GetGPUVirtualAddress());
    commands->SetGraphicsRootConstantBufferView(1,common_->GetShadowSceneBufferAddress());
    commands->SetGraphicsRootDescriptorTable(2,common_->GetShadowMapSrvHandle());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(1,1,&instancesView_);
    // One far-LOD instance draw; reflected grass does not duplicate the detailed near mesh.
    farModel_->Draw(commands,count_);
}
void TitleGrassRenderer::Finalize() noexcept {
    ready_ = false;
    if (mainConstants_ && mainMapped_) mainConstants_->Unmap(0,nullptr);
    if (shadowConstants_ && shadowMapped_) shadowConstants_->Unmap(0,nullptr);
    if (reflectionConstants_ && reflectionMapped_) reflectionConstants_->Unmap(0,nullptr);
    mainMapped_ = shadowMapped_ = reflectionMapped_ = nullptr;
    mainConstants_.Reset(); shadowConstants_.Reset(); reflectionConstants_.Reset(); instances_.Reset();
    mainPipeline_.Reset(); shadowPipeline_.Reset(); reflectionPipeline_.Reset(); root_.Reset();
    model_ = farModel_ = nullptr; common_ = nullptr; count_ = shadowCount_ = 0; instancesView_ = {};
}
}
