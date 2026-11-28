#pragma once
#include "3d/Object3dCommon.h"
#include "3d/ObjectBufferLayout.h"
#include "base/DirectXCommon.h"

namespace farm::trial {
static_assert(offsetof(graphics::ObjectMaterial,color)==0 && offsetof(graphics::ObjectMaterial,shininess)==20
    && offsetof(graphics::ObjectMaterial,surfaceFront)==128); // Routed PS c0, c1.y and c8.
// Uses the ordinary static model VB/CBs. Only the color target is blended; normals and depth stay opaque.
enum class WaterShader { Shaded, Routed, ActiveFaces, NoLanes };
inline Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateWaterPass(Object3dCommon& common,WaterShader shader=WaterShader::Shaded) {
    auto* dx=common.GetDxCommon();
    auto vs=dx->CompileShader(L"Resources/shader/Object3D.VS.hlsl",L"vs_6_0");
    const wchar_t* path=L"Resources/shader/TrialWater.PS.hlsl";
    switch(shader) {
    case WaterShader::Routed:path=L"Resources/shader/TrialRoutedWater.PS.hlsl";break;
    case WaterShader::ActiveFaces:path=L"Resources/shader/TrialActiveWater.PS.hlsl";break;
    case WaterShader::NoLanes:path=L"Resources/shader/TrialNoLaneWater.PS.hlsl";break;
    case WaterShader::Shaded:break;
    }
    auto ps=dx->CompileShader(path,L"ps_6_0");
    if(!vs || !ps) return {};
    const D3D12_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature=common.GetRootSignature(false);
    d.InputLayout={layout,3}; d.VS={vs->GetBufferPointer(),vs->GetBufferSize()}; d.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID; d.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;
    d.RasterizerState.DepthClipEnable=TRUE; d.BlendState.IndependentBlendEnable=TRUE;
    auto& blend=d.BlendState.RenderTarget[0];
    blend.BlendEnable=TRUE; blend.SrcBlend=D3D12_BLEND_SRC_ALPHA; blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD; blend.SrcBlendAlpha=D3D12_BLEND_ONE;
    blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA; blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    d.DepthStencilState.DepthEnable=TRUE; d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    d.NumRenderTargets=2; d.RTVFormats[0]=d.RTVFormats[1]=DXGI_FORMAT_R8G8B8A8_UNORM;
    d.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT; d.SampleDesc.Count=1; d.SampleMask=UINT_MAX;
    d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> result;
    if(FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&result)))) return {};
    return result;
}
}
