#include "2d/TextureManager.h"

#include "base/DirectXCommon.h"
#include "base/LoadTiming.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <cassert>
#include <cwctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

TextureManager* TextureManager::GetInstance() {
    static TextureManager instance;
    return &instance;
}

TextureManager::~TextureManager() {
    Finalize();
}

bool TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    if (initialized_) {
        Logger::Log("TextureManager::Initialize ignored because the manager is already initialized.");
        return dxCommon_ == dxCommon && srvManager_ == srvManager;
    }
    if (dxCommon == nullptr || srvManager == nullptr) {
        Logger::Log("TextureManager::Initialize failed. Dependencies must not be null.");
        return false;
    }

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    ownerThread_ = std::this_thread::get_id();

    ID3D12Device* device = dxCommon_->GetDevice();
    HRESULT hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&uploadCommandAllocator_));
    if (FAILED(hr)) {
        LogFailure("CreateCommandAllocator", hr);
        Finalize();
        return false;
    }

    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadCommandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&uploadCommandList_));
    if (FAILED(hr)) {
        LogFailure("CreateCommandList", hr);
        Finalize();
        return false;
    }
    hr = uploadCommandList_->Close();
    if (FAILED(hr)) {
        LogFailure("Close initial upload command list", hr);
        Finalize();
        return false;
    }

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence_));
    if (FAILED(hr)) {
        LogFailure("CreateFence", hr);
        Finalize();
        return false;
    }

    uploadFenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (uploadFenceEvent_ == nullptr) {
        Logger::Log("TextureManager::Initialize failed. CreateEvent returned null.");
        Finalize();
        return false;
    }

    initialized_ = true;
    if (!CreateFallbackTextures()) {
        Logger::Log("TextureManager::Initialize failed. Fallback textures could not be created.");
        Finalize();
        return false;
    }
    return true;
}

void TextureManager::Finalize() {
    if (initialized_ && !IsOwnerThread()) {
        Logger::Log("TextureManager::Finalize rejected. It must run on the initialization thread.");
        assert(false && "TextureManager thread ownership violation");
        return;
    }
	if (srvManager_ != nullptr) {
		for (const auto& [descriptorIndex, record] : records_) {
			static_cast<void>(record);
			if (srvManager_->IsAllocated(descriptorIndex)) {
				srvManager_->Release(descriptorIndex);
			}
		}
	}
    records_.clear();
    texture2DCache_.clear();
    textureCubeCache_.clear();
    fallback2D_ = {};
    fallbackCube_ = {};
    estimatedGpuBytes_ = 0;

    uploadFence_.Reset();
    uploadCommandList_.Reset();
    uploadCommandAllocator_.Reset();
    if (uploadFenceEvent_ != nullptr) {
        CloseHandle(uploadFenceEvent_);
        uploadFenceEvent_ = nullptr;
    }

    uploadFenceValue_ = 0;
    ownerThread_ = {};
    srvManager_ = nullptr;
    dxCommon_ = nullptr;
    initialized_ = false;
}

Texture2DHandle TextureManager::LoadTexture2D(const std::string& filePath, Lifetime lifetime, MipPolicy mipPolicy) {
    if (!initialized_ || !IsOwnerThread()) {
        Logger::Log("TextureManager::LoadTexture2D rejected. Manager is unavailable or called from a non-owner thread.");
        assert(initialized_ && IsOwnerThread());
        return fallback2D_;
    }
    if (filePath.empty()) {
        Logger::Log("TextureManager::LoadTexture2D used the fallback because the path is empty.");
        return fallback2D_;
    }

    const std::filesystem::path path = std::filesystem::path(filePath).lexically_normal();
    std::wstring key = MakeCacheKey(path);
    // '|' is invalid in a Windows file name; policies cannot alias an actual path.
    if (mipPolicy == MipPolicy::PreserveFile) key += L"|preserve-mips";
    if (const auto found = texture2DCache_.find(key); found != texture2DCache_.end()) {
		if (TextureRecord* record = FindRecord(
			found->second.Index(), found->second.Generation(), TextureKind::Texture2D);
			record != nullptr && lifetime == Lifetime::Global) {
			record->lifetime = Lifetime::Global;
		}
        return found->second;
    }

    DirectX::ScratchImage image;
    if (!LoadImage(path, TextureKind::Texture2D, image, mipPolicy)) {
        Logger::Log("TextureManager::LoadTexture2D used the fallback: " + filePath);
        return fallback2D_;
    }

    uint32_t srvIndex = SrvManager::kInvalidIndex;
    if (!RegisterTexture(image, path, TextureKind::Texture2D, lifetime, srvIndex)) {
        Logger::Log("TextureManager::LoadTexture2D failed to register the texture: " + filePath);
        return fallback2D_;
    }

    const Texture2DHandle handle(srvIndex, records_.at(srvIndex).generation);
    texture2DCache_.emplace(key, handle);
    return handle;
}

TextureCubeHandle TextureManager::LoadTextureCube(const std::string& filePath, Lifetime lifetime) {
    if (!initialized_ || !IsOwnerThread()) {
        Logger::Log("TextureManager::LoadTextureCube rejected. Manager is unavailable or called from a non-owner thread.");
        assert(initialized_ && IsOwnerThread());
        return fallbackCube_;
    }
    if (filePath.empty()) {
        Logger::Log("TextureManager::LoadTextureCube used the fallback because the path is empty.");
        return fallbackCube_;
    }

    const std::filesystem::path path = std::filesystem::path(filePath).lexically_normal();
    const std::wstring key = MakeCacheKey(path);
    if (const auto found = textureCubeCache_.find(key); found != textureCubeCache_.end()) {
		if (TextureRecord* record = FindRecord(
			found->second.Index(), found->second.Generation(), TextureKind::TextureCube);
			record != nullptr && lifetime == Lifetime::Global) {
			record->lifetime = Lifetime::Global;
		}
        return found->second;
    }

    DirectX::ScratchImage image;
    if (!LoadImage(path, TextureKind::TextureCube, image)) {
        Logger::Log("TextureManager::LoadTextureCube used the fallback: " + filePath);
        return fallbackCube_;
    }

    uint32_t srvIndex = SrvManager::kInvalidIndex;
    if (!RegisterTexture(image, path, TextureKind::TextureCube, lifetime, srvIndex)) {
        Logger::Log("TextureManager::LoadTextureCube failed to register the texture: " + filePath);
        return fallbackCube_;
    }

    const TextureCubeHandle handle(srvIndex, records_.at(srvIndex).generation);
    textureCubeCache_.emplace(key, handle);
    return handle;
}

Texture2DHandle TextureManager::CreateSolidColorTexture2D(
    const std::string& assetName,
    const std::array<uint8_t, 4>& color) {
    if (!initialized_ || !IsOwnerThread() || assetName.empty()) {
        Logger::Log("TextureManager::CreateSolidColorTexture2D rejected invalid input or thread ownership.");
        return fallback2D_;
    }

    std::wstring key = L"generated://";
    key.append(assetName.begin(), assetName.end());
    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    if (const auto found = texture2DCache_.find(key); found != texture2DCache_.end()) {
        return found->second;
    }

    DirectX::ScratchImage image;
    const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
    if (FAILED(hr)) {
        LogFailure("create a solid-color texture", hr);
        return fallback2D_;
    }
    uint8_t* pixels = image.GetPixels();
    std::copy(color.begin(), color.end(), pixels);

    uint32_t srvIndex = SrvManager::kInvalidIndex;
    if (!RegisterTexture(
		image,
		std::filesystem::path(assetName),
		TextureKind::Texture2D,
		Lifetime::Global,
		srvIndex)) {
        return fallback2D_;
    }
    const Texture2DHandle handle(srvIndex, records_.at(srvIndex).generation);
    texture2DCache_.emplace(std::move(key), handle);
    return handle;
}

Texture2DHandle TextureManager::UpdateUiTexture(const std::string& assetName, DirectX::ScratchImage& image) {
    const auto& metadata = image.GetMetadata();
    if (!initialized_ || !IsOwnerThread() || assetName.empty() || assetName.size() > 96 ||
        metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
        metadata.mipLevels != 1 || metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        metadata.width == 0 || metadata.height == 0 || metadata.width > 2048 || metadata.height > 256) return {};
    const std::wstring key = L"dynamic-ui://" + std::wstring(assetName.begin(), assetName.end());
    if (const auto found = texture2DCache_.find(key); found != texture2DCache_.end()) {
        auto* record = FindRecord(found->second.Index(), found->second.Generation(), TextureKind::Texture2D);
        if (!record || record->desc.Width != metadata.width || record->desc.Height != metadata.height) return {};
        // Same DIRECT queue orders prior draws before this copy; UploadTexture waits its fence.
        // No descriptor/resource replacement, so cached Sprite handles remain valid.
        return UploadTexture(record->resource.Get(), image, true) ? found->second : Texture2DHandle{};
    }
    uint32_t index = SrvManager::kInvalidIndex;
    if (!RegisterTexture(image, {}, TextureKind::Texture2D, Lifetime::Scene, index)) return {};
    const Texture2DHandle handle(index, records_.at(index).generation);
    texture2DCache_.emplace(key, handle);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGpuHandle(Texture2DHandle handle) const {
    uint32_t descriptorIndex = handle.Index();
	uint32_t generation = handle.Generation();
    if (FindRecord(descriptorIndex, generation, TextureKind::Texture2D) == nullptr) {
        descriptorIndex = fallback2D_.Index();
		generation = fallback2D_.Generation();
    }
    return FindRecord(descriptorIndex, generation, TextureKind::Texture2D) != nullptr
        ? srvManager_->GetGPUDescriptorHandle(descriptorIndex)
        : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGpuHandle(TextureCubeHandle handle) const {
    uint32_t descriptorIndex = handle.Index();
	uint32_t generation = handle.Generation();
    if (FindRecord(descriptorIndex, generation, TextureKind::TextureCube) == nullptr) {
        descriptorIndex = fallbackCube_.Index();
		generation = fallbackCube_.Generation();
    }
    return FindRecord(descriptorIndex, generation, TextureKind::TextureCube) != nullptr
        ? srvManager_->GetGPUDescriptorHandle(descriptorIndex)
        : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

D3D12_RESOURCE_DESC TextureManager::GetResourceDesc(Texture2DHandle handle) const {
    const TextureRecord* record = FindRecord(handle.Index(), handle.Generation(), TextureKind::Texture2D);
    if (record == nullptr) {
        record = FindRecord(
			fallback2D_.Index(), fallback2D_.Generation(), TextureKind::Texture2D);
    }
    return record != nullptr ? record->desc : D3D12_RESOURCE_DESC{};
}

Texture2DHandle TextureManager::GetTexture2DHandle(uint32_t descriptorIndex) const {
    const auto found = records_.find(descriptorIndex);
    if (found == records_.end() || found->second.kind != TextureKind::Texture2D) {
        return fallback2D_;
    }

    return Texture2DHandle(descriptorIndex, found->second.generation);
}

TextureManager::Statistics TextureManager::GetStatistics() const noexcept {
    Statistics statistics{};
    statistics.estimatedGpuBytes = estimatedGpuBytes_;
    for (const auto& [descriptorIndex, record] : records_) {
        static_cast<void>(descriptorIndex);
        if (record.kind == TextureKind::Texture2D) {
            ++statistics.texture2DCount;
        } else {
            ++statistics.textureCubeCount;
        }
    }
    return statistics;
}

void TextureManager::ReleaseSceneTextures() {
	if (!initialized_ || !IsOwnerThread()) {
		Logger::Log("TextureManager::ReleaseSceneTextures rejected invalid lifecycle or thread ownership.");
		assert(initialized_ && IsOwnerThread());
		return;
	}

	for (auto recordIterator = records_.begin(); recordIterator != records_.end();) {
		const uint32_t descriptorIndex = recordIterator->first;
		const TextureRecord& record = recordIterator->second;
		if (record.lifetime != Lifetime::Scene) {
			++recordIterator;
			continue;
		}

		if (record.kind == TextureKind::Texture2D) {
			const Texture2DHandle releasedHandle(descriptorIndex, record.generation);
			for (auto cacheIterator = texture2DCache_.begin(); cacheIterator != texture2DCache_.end();) {
				cacheIterator = cacheIterator->second == releasedHandle
					? texture2DCache_.erase(cacheIterator)
					: std::next(cacheIterator);
			}
		} else {
			const TextureCubeHandle releasedHandle(descriptorIndex, record.generation);
			for (auto cacheIterator = textureCubeCache_.begin(); cacheIterator != textureCubeCache_.end();) {
				cacheIterator = cacheIterator->second == releasedHandle
					? textureCubeCache_.erase(cacheIterator)
					: std::next(cacheIterator);
			}
		}

		estimatedGpuBytes_ -= (std::min)(estimatedGpuBytes_, record.estimatedGpuBytes);
		if (srvManager_->IsAllocated(descriptorIndex)) {
			srvManager_->Release(descriptorIndex);
		}
		recordIterator = records_.erase(recordIterator);
	}
}

bool TextureManager::IsOwnerThread() const noexcept {
    return ownerThread_ == std::this_thread::get_id();
}

std::wstring TextureManager::MakeCacheKey(const std::filesystem::path& filePath) const {
    std::error_code error;
    std::filesystem::path absolutePath = std::filesystem::absolute(filePath, error);
    if (error) {
        absolutePath = filePath;
    }
    std::wstring key = absolutePath.lexically_normal().generic_wstring();
    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return key;
}

bool TextureManager::LoadImage(
    const std::filesystem::path& filePath,
    TextureKind expectedKind,
    DirectX::ScratchImage& outImage,
    MipPolicy mipPolicy) const {
    std::error_code error;
    if (!std::filesystem::is_regular_file(filePath, error) || error) {
        Logger::Log("TextureManager could not find a texture file: " + filePath.generic_string());
        return false;
    }

    std::wstring extension = filePath.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });

    DirectX::TexMetadata metadata{};
    HRESULT hr = E_FAIL;
    loading::Scope decodeTiming(loading::Stage::TextureDecode);
    if (extension == L".dds") {
        hr = DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, outImage);
    } else {
        hr = DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata, outImage);
    }
    decodeTiming.Stop();
    if (FAILED(hr)) {
        LogFailure("load " + filePath.generic_string(), hr);
        return false;
    }

    const bool isCube = metadata.IsCubemap();
    if ((expectedKind == TextureKind::TextureCube) != isCube) {
        Logger::Log("TextureManager rejected a texture whose view type does not match the requested handle: " + filePath.generic_string());
        return false;
    }
    if (expectedKind == TextureKind::Texture2D && metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D) {
        Logger::Log("TextureManager rejected a non-2D texture: " + filePath.generic_string());
        return false;
    }

    if (mipPolicy == MipPolicy::GenerateIfMissing && expectedKind == TextureKind::Texture2D &&
        metadata.mipLevels == 1 && metadata.width > 1 && metadata.height > 1) {
        loading::Scope mipTiming(loading::Stage::TextureMips);
        DirectX::ScratchImage mipChain;
        hr = DirectX::GenerateMipMaps(
            outImage.GetImages(),
            outImage.GetImageCount(),
            outImage.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipChain);
        if (SUCCEEDED(hr)) {
            outImage = std::move(mipChain);
        } else {
            Logger::Log("TextureManager continued without generated mipmaps: " + filePath.generic_string());
        }
    }
    return true;
}

bool TextureManager::CreateFallbackTextures() {
    DirectX::ScratchImage checker;
    HRESULT hr = checker.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 2, 2, 1, 1);
    if (FAILED(hr)) {
        LogFailure("create 2D fallback image", hr);
        return false;
    }

    const DirectX::Image* checkerImage = checker.GetImage(0, 0, 0);
    constexpr uint32_t kMagenta = 0xffff00ffu;
    constexpr uint32_t kBlack = 0xff000000u;
    for (std::size_t y = 0; y < 2; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(checkerImage->pixels + checkerImage->rowPitch * y);
        for (std::size_t x = 0; x < 2; ++x) {
            row[x] = ((x + y) % 2 == 0) ? kMagenta : kBlack;
        }
    }

    uint32_t fallback2DIndex = SrvManager::kInvalidIndex;
    if (!RegisterTexture(checker, {}, TextureKind::Texture2D, Lifetime::Global, fallback2DIndex)) {
        return false;
    }
    fallback2D_ = Texture2DHandle(fallback2DIndex, records_.at(fallback2DIndex).generation);

    DirectX::ScratchImage cube;
    hr = cube.InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
    if (FAILED(hr)) {
        LogFailure("create cube fallback image", hr);
        return false;
    }
    for (std::size_t imageIndex = 0; imageIndex < cube.GetImageCount(); ++imageIndex) {
        *reinterpret_cast<uint32_t*>(cube.GetImages()[imageIndex].pixels) = kBlack;
    }

    uint32_t fallbackCubeIndex = SrvManager::kInvalidIndex;
    if (!RegisterTexture(cube, {}, TextureKind::TextureCube, Lifetime::Global, fallbackCubeIndex)) {
        return false;
    }
    fallbackCube_ = TextureCubeHandle(fallbackCubeIndex, records_.at(fallbackCubeIndex).generation);
    return true;
}

bool TextureManager::RegisterTexture(
    DirectX::ScratchImage& image,
    const std::filesystem::path& sourcePath,
    TextureKind kind,
	Lifetime lifetime,
    uint32_t& outSrvIndex) {
    ComPtr<ID3D12Resource> resource = CreateTextureResource(image.GetMetadata());
    if (resource == nullptr || !UploadTexture(resource.Get(), image)) {
        return false;
    }

    const uint32_t srvIndex = srvManager_->Allocate();
    if (srvIndex == SrvManager::kInvalidIndex) {
        Logger::Log("TextureManager could not allocate an SRV descriptor.");
        return false;
    }

    const DirectX::TexMetadata& metadata = image.GetMetadata();
    if (kind == TextureKind::Texture2D) {
        srvManager_->CreateSRVforTexture2D(
            srvIndex,
            resource.Get(),
            metadata.format,
            static_cast<UINT>(metadata.mipLevels));
    } else {
        srvManager_->CreateSRVforTextureCube(
            srvIndex,
            resource.Get(),
            metadata.format,
            static_cast<UINT>(metadata.mipLevels));
    }

    TextureRecord record{};
    record.resource = std::move(resource);
    record.desc = record.resource->GetDesc();
    record.sourcePath = sourcePath;
    record.kind = kind;
	record.lifetime = lifetime;
	uint32_t& generation = descriptorGenerations_[srvIndex];
	++generation;
	if (generation == 0) {
		++generation;
	}
	record.generation = generation;
    record.estimatedGpuBytes = EstimateGpuBytes(metadata);
    estimatedGpuBytes_ += record.estimatedGpuBytes;
    records_.emplace(srvIndex, std::move(record));
    outSrvIndex = srvIndex;
    return true;
}

ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) const {
    loading::Scope timing(loading::Stage::TextureResource);
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    desc.Alignment = 0;
    desc.Width = static_cast<UINT64>(metadata.width);
    desc.Height = static_cast<UINT>(metadata.height);
    desc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    desc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    desc.Format = metadata.format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> resource;
    const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        LogFailure("CreateCommittedResource for texture", hr);
        return nullptr;
    }
    return resource;
}

ComPtr<ID3D12Resource> TextureManager::CreateUploadBuffer(uint64_t sizeInBytes) const {
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        LogFailure("CreateCommittedResource for upload buffer", hr);
        return nullptr;
    }
    return resource;
}

bool TextureManager::UploadTexture(ID3D12Resource* texture, const DirectX::ScratchImage& image, bool previouslySampled) {
    loading::Scope cpuTiming(loading::Stage::TextureUploadCpu);
    if (texture == nullptr) {
        return false;
    }

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    HRESULT hr = DirectX::PrepareUpload(
        dxCommon_->GetDevice(),
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        subresources);
    if (FAILED(hr) || subresources.empty()) {
        LogFailure("PrepareUpload", hr);
        return false;
    }

    const uint64_t uploadSize = GetRequiredIntermediateSize(
        texture,
        0,
        static_cast<UINT>(subresources.size()));
    ComPtr<ID3D12Resource> uploadBuffer = CreateUploadBuffer(uploadSize);
    if (uploadBuffer == nullptr) {
        return false;
    }

    hr = uploadCommandAllocator_->Reset();
    if (FAILED(hr)) {
        LogFailure("reset upload command allocator", hr);
        return false;
    }
    hr = uploadCommandList_->Reset(uploadCommandAllocator_.Get(), nullptr);
    if (FAILED(hr)) {
        LogFailure("reset upload command list", hr);
        return false;
    }

    if (previouslySampled) {
        D3D12_RESOURCE_BARRIER before{};
        before.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        before.Transition.pResource = texture;
        before.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        before.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        before.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        uploadCommandList_->ResourceBarrier(1, &before);
    }
    UpdateSubresources(
        uploadCommandList_.Get(),
        texture,
        uploadBuffer.Get(),
        0,
        0,
        static_cast<UINT>(subresources.size()),
        subresources.data());

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    uploadCommandList_->ResourceBarrier(1, &barrier);

    hr = uploadCommandList_->Close();
    if (FAILED(hr)) {
        LogFailure("close upload command list", hr);
        return false;
    }

    cpuTiming.Stop();
    loading::Scope waitTiming(loading::Stage::TextureSubmitWait);
    ID3D12CommandList* commandLists[] = { uploadCommandList_.Get() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    ++uploadFenceValue_;
    hr = dxCommon_->GetCommandQueue()->Signal(uploadFence_.Get(), uploadFenceValue_);
    if (FAILED(hr)) {
        LogFailure("signal upload fence", hr);
        return false;
    }
    if (uploadFence_->GetCompletedValue() < uploadFenceValue_) {
        hr = uploadFence_->SetEventOnCompletion(uploadFenceValue_, uploadFenceEvent_);
        if (FAILED(hr)) {
            LogFailure("set upload fence event", hr);
            return false;
        }
        const DWORD waitResult = WaitForSingleObject(uploadFenceEvent_, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            Logger::Log("TextureManager upload wait failed.");
            return false;
        }
    }
    return true;
}

const TextureManager::TextureRecord* TextureManager::FindRecord(
    uint32_t descriptorIndex,
	uint32_t generation,
    TextureKind kind) const {
    if (!initialized_ || descriptorIndex == SrvManager::kInvalidIndex) {
        return nullptr;
    }
    const auto found = records_.find(descriptorIndex);
    if (found == records_.end() ||
		found->second.kind != kind ||
		found->second.generation != generation) {
        return nullptr;
    }
    return &found->second;
}

TextureManager::TextureRecord* TextureManager::FindRecord(
	uint32_t descriptorIndex,
	uint32_t generation,
	TextureKind kind) {
	return const_cast<TextureRecord*>(std::as_const(*this).FindRecord(
		descriptorIndex, generation, kind));
}

uint64_t TextureManager::EstimateGpuBytes(const DirectX::TexMetadata& metadata) const noexcept {
    uint64_t totalBytes = 0;
    std::size_t width = metadata.width;
    std::size_t height = metadata.height;
    for (std::size_t mip = 0; mip < metadata.mipLevels; ++mip) {
        std::size_t rowPitch = 0;
        std::size_t slicePitch = 0;
        if (SUCCEEDED(DirectX::ComputePitch(metadata.format, width, height, rowPitch, slicePitch))) {
            totalBytes += static_cast<uint64_t>(slicePitch) * metadata.arraySize;
        }
        width = (std::max)(std::size_t{ 1 }, width / 2);
        height = (std::max)(std::size_t{ 1 }, height / 2);
    }
    return totalBytes;
}

void TextureManager::LogFailure(const std::string& operation, HRESULT result) const {
    std::ostringstream stream;
    stream << "TextureManager failed to " << operation << ". HRESULT=0x"
        << std::hex << std::uppercase << static_cast<uint32_t>(result);
    Logger::Log(stream.str());
}
