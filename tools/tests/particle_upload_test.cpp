#include "effect/ParticleUploadMapping.h"
#include <iostream>

struct UploadStub {
    HRESULT result;
    void* memory;
    HRESULT Map(UINT subresource,const D3D12_RANGE* range,void** output) {
        if(subresource!=0 || !range || range->Begin!=0 || range->End!=0) return E_INVALIDARG;
        *output=memory;
        return result;
    }
};
int main() {
    int old=1,buffer=2;
    UploadStub failure{E_OUTOFMEMORY,&buffer},empty{S_OK,nullptr},ok{S_OK,&buffer};
    for(auto* stub:{static_cast<UploadStub*>(nullptr),&failure,&empty}) {
        int* out=&old;
        try { particle::MapUpload(stub,out,"test"); return 1; }
        catch(const graphics::InitializationError&) { if(out!=&old) return 2; }
    }
    int* out=nullptr;
    particle::MapUpload(&ok,out,"test");
    if(out!=&buffer) return 3;
    std::cout<<"PASS particle Map: null allocation, failing HRESULT, null output, successful mapping\n";
}
