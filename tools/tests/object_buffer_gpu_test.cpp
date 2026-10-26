#include "3d/ObjectBufferLayout.h"
#include <wrl.h>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <array>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
using Microsoft::WRL::ComPtr;
using graphics::ObjectBufferData;
void Check(bool ok) { if (!ok) throw std::runtime_error("Object buffer GPU validation failed"); }
void HR(HRESULT value) { Check(SUCCEEDED(value)); }
ComPtr<ID3D12Resource> Buffer(ID3D12Device* device, UINT64 bytes, D3D12_HEAP_TYPE type,
    D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = type;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = bytes;
    desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags = flags;
    ComPtr<ID3D12Resource> buffer;
    HR(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&buffer)));
    return buffer;
}
int main() try {
    ComPtr<ID3D12Debug> debug;
    const bool validation = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    if (validation) debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    HR(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    ComPtr<ID3D12InfoQueue> info;
    if (validation) HR(device.As(&info));
    constexpr unsigned count = 8, fields = 11;
    std::array<ComPtr<ID3D12Resource>,count> resources;
    std::array<std::array<Vector4,fields>,count> expected{};
    const D3D12_RANGE noRead{0,0};
    for (unsigned i = 0; i < count; ++i) {
        resources[i] = Buffer(device.Get(),sizeof(ObjectBufferData),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped = nullptr; HR(resources[i]->Map(0,&noRead,&mapped)); Check(mapped != nullptr);
        auto* data = ::new(mapped) ObjectBufferData{};
        for (unsigned j = 0; j < fields; ++j) {
            const float base = float(i*100+j*4);
            expected[i][j] = {base,base+1,base+2,base+3};
        }
        data->material.color = expected[i][0];
        data->material.enableLighting = static_cast<int>(expected[i][1].x);
        data->material.shininess = expected[i][1].y;
        data->material.environmentCoefficient = expected[i][1].z;
        data->material.specularType = static_cast<int>(expected[i][1].w);
        std::memcpy(data->material.uvTransform.m[2],&expected[i][2],sizeof(Vector4));
        data->material.surfaceDetail = expected[i][3];
        data->material.outdoorAmbient = expected[i][4];
        std::memcpy(data->transform.WVP.m[1],&expected[i][5],sizeof(Vector4));
        std::memcpy(data->transform.World.m[2],&expected[i][6],sizeof(Vector4));
        std::memcpy(data->transform.WorldInverseTranspose.m[3],&expected[i][7],sizeof(Vector4));
        std::memcpy(data->shadow.WVP.m[1],&expected[i][8],sizeof(Vector4));
        std::memcpy(data->shadow.World.m[2],&expected[i][9],sizeof(Vector4));
        std::memcpy(data->shadow.WorldInverseTranspose.m[3],&expected[i][10],sizeof(Vector4));
        const auto address = resources[i]->GetGPUVirtualAddress();
        for (const auto offset : {offsetof(ObjectBufferData,material),offsetof(ObjectBufferData,transform),offsetof(ObjectBufferData,shadow)})
            Check((address+offset)%D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
        for (unsigned j = 0; j < i; ++j) Check(address != resources[j]->GetGPUVirtualAddress());
    }
    const char shader[] = R"(
#include "Object3d.hlsli"
cbuffer Surface : register(b0) { Material mat; };
cbuffer MainTransform : register(b1) { float4x4 wvp; float4x4 world; float4x4 normal; };
cbuffer ShadowTransform : register(b2) { float4x4 swvp; float4x4 sworld; float4x4 snormal; };
RWStructuredBuffer<float4> results : register(u0);
cbuffer Index : register(b3) { uint index; };
[numthreads(1,1,1)] void main() {
 uint p=index*11;
 results[p]=mat.color;
 results[p+1]=float4(mat.enableLighting,mat.shininess,mat.environmentCoefficient,mat.specularType);
 results[p+2]=mat.uvTransform[2]; results[p+3]=mat.surfaceDetail; results[p+4]=mat.outdoorAmbient;
 results[p+5]=wvp[1]; results[p+6]=world[2]; results[p+7]=normal[3];
 results[p+8]=swvp[1]; results[p+9]=sworld[2]; results[p+10]=snormal[3];
})";
    ComPtr<ID3DBlob> code, errors, serialized;
    const auto compiled = D3DCompile(shader,sizeof(shader),"project/Resources/shader/ObjectBufferValidation.hlsl",nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&code,&errors);
    if (FAILED(compiled) && errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
    HR(compiled);
    D3D12_ROOT_PARAMETER params[5]{};
    for (unsigned i = 0; i < 3; ++i) { params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; params[i].Descriptor.ShaderRegister = i; }
    params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[4].Constants.ShaderRegister = 3; params[4].Constants.Num32BitValues = 1;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{}; rootDesc.NumParameters = 5; rootDesc.pParameters = params;
    HR(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors));
    ComPtr<ID3D12RootSignature> root;
    HR(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = root.Get(); psoDesc.CS = {code->GetBufferPointer(),code->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso; HR(device->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&pso)));
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC queueDesc{};
    HR(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator;
    HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> list;
    HR(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),pso.Get(),IID_PPV_ARGS(&list)));
    constexpr auto resultBytes = sizeof(Vector4)*count*fields;
    auto output = Buffer(device.Get(),resultBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback = Buffer(device.Get(),resultBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    list->SetComputeRootSignature(root.Get());
    list->SetComputeRootUnorderedAccessView(3,output->GetGPUVirtualAddress());
    for (unsigned i = 0; i < count; ++i) {
        const auto address = resources[i]->GetGPUVirtualAddress();
        list->SetComputeRootConstantBufferView(0,address+offsetof(ObjectBufferData,material));
        list->SetComputeRootConstantBufferView(1,address+offsetof(ObjectBufferData,transform));
        list->SetComputeRootConstantBufferView(2,address+offsetof(ObjectBufferData,shadow));
        list->SetComputeRoot32BitConstant(4,i,0);
        list->Dispatch(1,1,1);
    }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = output.Get(); barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1,&barrier); list->CopyResource(readback.Get(),output.Get()); HR(list->Close());
    ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence; HR(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    HR(queue->Signal(fence.Get(),1));
    HANDLE event = CreateEvent(nullptr,FALSE,FALSE,nullptr); Check(event != nullptr);
    const auto eventResult = fence->SetEventOnCompletion(1,event);
    if (FAILED(eventResult)) { CloseHandle(event); HR(eventResult); }
    const auto wait = WaitForSingleObject(event,10000); CloseHandle(event); Check(wait == WAIT_OBJECT_0);
    void* mapped = nullptr; const D3D12_RANGE readRange{0,resultBytes};
    HR(readback->Map(0,&readRange,&mapped)); Check(mapped != nullptr);
    Check(std::memcmp(mapped,expected.data(),resultBytes) == 0);
    readback->Unmap(0,&noRead);
    for (auto& resource : resources) resource->Unmap(0,nullptr);
    if (info) for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
        SIZE_T bytes = 0; HR(info->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> storage(bytes); auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        HR(info->GetMessage(i,message,&bytes));
        Check(message->Severity != D3D12_MESSAGE_SEVERITY_ERROR && message->Severity != D3D12_MESSAGE_SEVERITY_CORRUPTION);
    }
    std::cout << "Object buffer: 8 independent resources, 11 material/main/shadow fields, actual HLSL Material, GPU readback PASS; debugLayer=" << validation << '\n';
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
