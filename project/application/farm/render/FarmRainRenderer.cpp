#include "farm/render/FarmRainRenderer.h"
#include "base/DirectXCommon.h"
#include "3d/Camera.h"

bool FarmRainRenderer::Initialize(DirectXCommon* dx) {
    if (pipeline_) return true;
    if (!dx || !dx->GetDevice()) return false;
    const auto vs=dx->CompileShader(L"Resources/shader/LineVS.hlsl",L"vs_6_0");
    const auto ps=dx->CompileShader(L"Resources/shader/LinePS.hlsl",L"ps_6_0");
    if (!vs || !ps) return false;
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.NumParameters=1; signature.pParameters=&parameter;
    signature.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error)) ||
        FAILED(dx->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)))) return false;
    const D3D12_INPUT_ELEMENT_DESC input[]{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature=root.Get(); pso.VS={vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS={ps->GetBufferPointer(),ps->GetBufferSize()}; pso.InputLayout={input,2};
    pso.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode=D3D12_CULL_MODE_NONE; pso.RasterizerState.DepthClipEnable=TRUE;
    auto& blend=pso.BlendState.RenderTarget[0];
    blend.BlendEnable=TRUE; blend.SrcBlend=D3D12_BLEND_SRC_ALPHA; blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp=D3D12_BLEND_OP_ADD; blend.SrcBlendAlpha=D3D12_BLEND_ONE;
    blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA; blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable=TRUE; pso.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT; pso.NumRenderTargets=1;
    pso.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count=1; pso.SampleMask=D3D12_DEFAULT_SAMPLE_MASK;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    if (FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline)))) return false;
    auto vertices=dx->CreateBufferResource(sizeof(farm::rain::Vertex)*farm::rain::kVertexCount);
    auto constants=dx->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (!vertices || !constants) return false;
    const D3D12_RANGE noRead{0,0};
    farm::rain::Vertex* mappedVertices=nullptr; Constants* mappedConstants=nullptr;
    if (FAILED(vertices->Map(0,&noRead,reinterpret_cast<void**>(&mappedVertices))) ||
        FAILED(constants->Map(0,&noRead,reinterpret_cast<void**>(&mappedConstants)))) return false;
    *mappedConstants={MatrixMath::MakeIdentity4x4(),MatrixMath::MakeIdentity4x4()};
    view_={vertices->GetGPUVirtualAddress(),static_cast<UINT>(sizeof(farm::rain::Vertex)*farm::rain::kVertexCount),sizeof(farm::rain::Vertex)};
    root_=std::move(root); pipeline_=std::move(pipeline);
    vertices_=std::move(vertices); constants_=std::move(constants);
    mappedVertices_=mappedVertices; mappedConstants_=mappedConstants; dx_=dx;
    return true;
}
void FarmRainRenderer::Draw(const farm::FarmRainFrame& frame, const Camera& camera, Vector3 center, float extentX, float extentZ) {
    if (!pipeline_ || !dx_ || frame.rain<=0) return;
    const auto& world=camera.GetWorldMatrix();
    const auto count=farm::rain::Build({mappedVertices_,farm::rain::kVertexCount},frame,center,
        {world.m[0][0],world.m[0][1],world.m[0][2]},extentX,extentZ);
    if (!count) return;
    // One upload/draw per frame. DirectXCommon::PostDraw waits for its Fence before reuse/destruction.
    mappedConstants_->viewProjection=camera.GetViewProjectionMatrix();
    auto* commands=dx_->GetCommandList();
    dx_->SetSceneRenderTarget();
    commands->SetGraphicsRootSignature(root_.Get()); commands->SetPipelineState(pipeline_.Get());
    commands->SetGraphicsRootConstantBufferView(0,constants_->GetGPUVirtualAddress());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0,1,&view_);
    commands->DrawInstanced(static_cast<UINT>(count),1,0,0);
}
