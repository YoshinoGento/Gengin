#include "2d/SpriteBufferLayout.h"
#include <wrl.h>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <array>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
using Microsoft::WRL::ComPtr;
using graphics::SpriteBufferData;
void Check(bool ok) { if (!ok) throw std::runtime_error("Sprite buffer GPU validation failed"); }
void HR(HRESULT value) { Check(SUCCEEDED(value)); }
ComPtr<ID3D12Resource> Buffer(ID3D12Device* device, UINT64 bytes, D3D12_HEAP_TYPE type,
    D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=type;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width=bytes;
    desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
    desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags=flags;
    ComPtr<ID3D12Resource> buffer;
    HR(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&buffer)));
    return buffer;
}
int main() try {
    ComPtr<ID3D12Debug> debug;
    const bool validation=SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    if(validation) debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    HR(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    ComPtr<ID3D12InfoQueue> info;
    if(validation) HR(device.As(&info));
    constexpr unsigned count=8;
    std::array<ComPtr<ID3D12Resource>,count> resources;
    std::array<Vector4,count> expected;
    const D3D12_RANGE noRead{0,0};
    for(unsigned i=0;i<count;++i) {
        resources[i]=Buffer(device.Get(),sizeof(SpriteBufferData),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped=nullptr; HR(resources[i]->Map(0,&noRead,&mapped)); Check(mapped!=nullptr);
        auto* data=::new(mapped) SpriteBufferData{};
        data->material.color={float(i),float(i+1),float(i+2),float(i+3)};
        for(unsigned j=0;j<4;++j) data->transform.World.m[0][j]=10.0f+float(j);
        expected[i]={float(i+10),float(i+12),float(i+14),float(i+16)};
        data->vertices[3].position={-7,-8,-9,-10};
        data->indices[5]=2;
        const auto address=resources[i]->GetGPUVirtualAddress();
        Check((address+offsetof(SpriteBufferData,material))%256==0);
        Check((address+offsetof(SpriteBufferData,transform))%256==0);
        for(unsigned j=0;j<i;++j) Check(address!=resources[j]->GetGPUVirtualAddress());
    }
    const char shader[]=R"(
cbuffer Material : register(b0) { float4 color; int lighting; float3 pad; row_major float4x4 uv; };
cbuffer Transform : register(b1) { row_major float4x4 wvp; row_major float4x4 world; };
RWStructuredBuffer<float4> results : register(u0);
cbuffer Index : register(b2) { uint index; };
[numthreads(1,1,1)] void main() { results[index]=color+world[0]; }
)";
    ComPtr<ID3DBlob> code, errors, serialized;
    HR(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors));
    D3D12_ROOT_PARAMETER params[4]{};
    params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV; params[0].Descriptor.ShaderRegister=0;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV; params[1].Descriptor.ShaderRegister=1;
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[3].Constants.ShaderRegister=2; params[3].Constants.Num32BitValues=1;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{}; rootDesc.NumParameters=4; rootDesc.pParameters=params;
    HR(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors));
    ComPtr<ID3D12RootSignature> root;
    HR(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature=root.Get(); psoDesc.CS={code->GetBufferPointer(),code->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso; HR(device->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&pso)));
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC queueDesc{};
    HR(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator;
    HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> list;
    HR(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),pso.Get(),IID_PPV_ARGS(&list)));
    const auto resultBytes=sizeof(Vector4)*count;
    auto output=Buffer(device.Get(),resultBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback=Buffer(device.Get(),resultBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    list->SetComputeRootSignature(root.Get());
    list->SetComputeRootUnorderedAccessView(2,output->GetGPUVirtualAddress());
    for(unsigned i=0;i<count;++i) {
        const auto address=resources[i]->GetGPUVirtualAddress();
        list->SetComputeRootConstantBufferView(0,address+offsetof(SpriteBufferData,material));
        list->SetComputeRootConstantBufferView(1,address+offsetof(SpriteBufferData,transform));
        list->SetComputeRoot32BitConstant(3,i,0);
        list->Dispatch(1,1,1);
    }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=output.Get(); barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1,&barrier); list->CopyResource(readback.Get(),output.Get()); HR(list->Close());
    ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence; HR(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    HR(queue->Signal(fence.Get(),1));
    HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr); Check(event!=nullptr);
    const auto eventResult=fence->SetEventOnCompletion(1,event);
    if (FAILED(eventResult)) { CloseHandle(event); HR(eventResult); }
    const auto wait=WaitForSingleObject(event,10000); CloseHandle(event); Check(wait==WAIT_OBJECT_0);
    void* mapped=nullptr; const D3D12_RANGE readRange{0,resultBytes};
    HR(readback->Map(0,&readRange,&mapped)); Check(mapped!=nullptr);
    const auto* values=static_cast<const Vector4*>(mapped);
    for(unsigned i=0;i<count;++i) {
        Check(values[i].x==expected[i].x && values[i].y==expected[i].y && values[i].z==expected[i].z && values[i].w==expected[i].w);
    }
    readback->Unmap(0,&noRead);
    if(info) for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i) {
        SIZE_T bytes=0; HR(info->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> storage(bytes); auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        HR(info->GetMessage(i,message,&bytes));
        Check(message->Severity!=D3D12_MESSAGE_SEVERITY_ERROR && message->Severity!=D3D12_MESSAGE_SEVERITY_CORRUPTION);
    }
    std::cout << "Sprite buffer: 8 independent packed resources, CBV alignment/field offsets, GPU readback PASS; debugLayer=" << validation << '\n';
    return 0;
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
