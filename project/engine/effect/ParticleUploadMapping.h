#pragma once
#include "base/D3D12DeviceSelection.h"
#include <string>

namespace particle {
// Destination is published only on success. The caller owns the mapped UPLOAD resource.
template<class Resource,class T> void MapUpload(Resource* resource,T*& destination,const char* label) {
    void* mapped=nullptr;
    D3D12_RANGE noRead{0,0};
    if(!resource || FAILED(resource->Map(0,&noRead,&mapped)) || !mapped)
        throw graphics::InitializationError(std::string("Particle buffer allocation/Map failed: ")+label);
    destination=static_cast<T*>(mapped);
}
}
