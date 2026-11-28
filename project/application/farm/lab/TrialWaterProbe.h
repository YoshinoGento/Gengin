#pragma once
#include "base/DirectXCommon.h"
#include <filesystem>
#include <fstream>

namespace farm::trial {
// Opt-in benchmark. Mark the previous submission on the direct queue before mapping or reusing its slots.
class WaterProbe {
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> time_,stats_;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    struct Result { UINT64 begin,end; D3D12_QUERY_DATA_PIPELINE_STATISTICS stats; };
    UINT64 frequency_=0,marker_=0;
    unsigned count_=0;
    bool recorded_=false,signaled_=false,enabled_=false;
    size_t draws_=0;
    char adapter_[512]="unknown";
    std::ofstream csv_;
public:
    bool Sampling() const noexcept { return enabled_; }
    void Initialize(DirectXCommon& dx,const char* variant) {
        auto* dev=dx.GetDevice();
        if(!dev || !dx.GetCommandQueue()) return;
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc{};
        if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->EnumAdapterByLuid(dev->GetAdapterLuid(),IID_PPV_ARGS(&adapter))) &&
            SUCCEEDED(adapter->GetDesc1(&desc)))
            WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,adapter_,sizeof(adapter_),nullptr,nullptr);
        D3D12_QUERY_HEAP_DESC q{D3D12_QUERY_HEAP_TYPE_TIMESTAMP,2,0};
        if(FAILED(dev->CreateQueryHeap(&q,IID_PPV_ARGS(&time_)))) return;
        q.Type=D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS; q.Count=1;
        if(FAILED(dev->CreateQueryHeap(&q,IID_PPV_ARGS(&stats_)))) return;
        D3D12_HEAP_PROPERTIES h{}; h.Type=D3D12_HEAP_TYPE_READBACK; h.CreationNodeMask=h.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC d{}; d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; d.Width=sizeof(Result);
        d.Height=d.DepthOrArraySize=d.MipLevels=d.SampleDesc.Count=1; d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if(FAILED(dev->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback_))) ||
            FAILED(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_))) ||
            FAILED(dx.GetCommandQueue()->GetTimestampFrequency(&frequency_)) || !frequency_) return;
        std::error_code ec; auto dir=std::filesystem::temp_directory_path(ec)/"SuiroWaterTrial";
        if(ec) return;
        std::filesystem::create_directories(dir,ec); if(ec) return;
        csv_.open(dir/(std::string("gpu-")+variant+"-"+std::to_string(GetCurrentProcessId())+".csv"));
        if(csv_) { csv_<<"sample,water_gpu_us,ps_invocations,water_draws,ia_vertices,vs_invocations,adapter\n"; enabled_=true; }
    }
    bool Begin(DirectXCommon& dx) {
        if(!enabled_) return false;
        if(recorded_) {
            if(!signaled_) {
                if(FAILED(dx.GetCommandQueue()->Signal(fence_.Get(),++marker_))) {enabled_=false;return false;}
                signaled_=true;
            }
            const auto done=fence_->GetCompletedValue();
            if(done==UINT64_MAX) {enabled_=false;return false;}
            if(done<marker_) return false;
            D3D12_RANGE range{0,sizeof(Result)}; Result* r=nullptr;
            if(FAILED(readback_->Map(0,&range,reinterpret_cast<void**>(&r)))) {enabled_=false;return false;}
            if(count_>=16 && r->end>=r->begin) csv_<<count_-16<<','<<(r->end-r->begin)*1.e6/frequency_<<','<<r->stats.PSInvocations<<','<<draws_<<','<<r->stats.IAVertices<<','<<r->stats.VSInvocations<<",\""<<adapter_<<"\"\n";
            D3D12_RANGE noWrite{}; readback_->Unmap(0,&noWrite); recorded_=signaled_=false;
            if(++count_==80) {csv_.close();enabled_=false;return false;}
        }
        auto* cmd=dx.GetCommandList();
        cmd->BeginQuery(stats_.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        cmd->EndQuery(time_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
        return true;
    }
    void End(DirectXCommon& dx,size_t draws) {
        auto* cmd=dx.GetCommandList();
        cmd->EndQuery(time_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
        cmd->EndQuery(stats_.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        cmd->ResolveQueryData(time_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,readback_.Get(),0);
        cmd->ResolveQueryData(stats_.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0,1,readback_.Get(),offsetof(Result,stats));
        recorded_=true; draws_=draws;
    }
};
}
