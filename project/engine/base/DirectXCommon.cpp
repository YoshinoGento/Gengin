#include "DirectXCommon.h"
#include "Logger.h"
#include "LoadTiming.h"
#include "D3D12DeviceSelection.h"
#include <cstdio>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

namespace {
void RequireGraphics(HRESULT result, const char* operation) {
    if (SUCCEEDED(result)) return;
    char message[256]{};
    std::snprintf(message, sizeof(message), "%s failed (HRESULT 0x%08lX).", operation,
        static_cast<unsigned long>(result));
    Logger::Error(message);
    throw graphics::InitializationError(message);
}
}

// --- 譁・ｭ怜・螟画鋤繝倥Ν繝代・ ---
std::wstring ConvertString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (sizeNeeded == 0) return std::wstring();
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) return std::string();
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], sizeNeeded, NULL, NULL);
    return result;
}

// --- 蛻晄悄蛹悶Γ繧､繝ｳ ---
void DirectXCommon::Initialize(WinApp* winApp) {
    assert(winApp);
    winApp_ = winApp;

    InitializeDevice();           // GPU縺ｮ貅門ｙ
    InitializeCommand();          // 蜻ｽ莉､繝ｪ繧ｹ繝医・貅門ｙ
    InitializeSwapChain(winApp);  // 逕ｻ髱｢蜈･繧梧崛縺郁ｨｭ螳・
    InitializeRenderTargetView(); // 謠上″霎ｼ縺ｿ蜈育ｮ｡逅・RTV)縺ｮ貅門ｙ
    InitializeDepthStencilView(); // 螂･陦後″(Z繝舌ャ繝輔ぃ)縺ｮ貅門ｙ
    InitializeRenderTexture();    // 笘・お繝・ぅ繧ｿ逕ｨ・壹ご繝ｼ繝逕ｻ髱｢逕ｨ繝・け繧ｹ繝√Ε縺ｮ菴懈・
    InitializeFence();            // CPU/GPU蜷梧悄縺ｮ貅門ｙ
    InitializeDXCCompiler();      // 繧ｷ繧ｧ繝ｼ繝繝ｼ鄙ｻ險ｳ讖溘・貅門ｙ
}

// --- 蛻晄悄蛹悶・隧ｳ邏ｰ螳溯｣・ｼ・NK2019蟇ｾ遲厄ｼ壹☆縺ｹ縺ｦ險倩ｿｰ・・---

void DirectXCommon::InitializeDevice() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif
    RequireGraphics(CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_)), "CreateDXGIFactory");
    auto selected = graphics::SelectHardwareDevice(dxgiFactory_.Get());
    RequireGraphics(selected.error, "DirectX12 hardware device (FL12_0+ and Shader Model6.0 required)");
    if (!selected.device) RequireGraphics(E_POINTER, "DirectX12 device selection");
    device_ = std::move(selected.device);
    char level[32]{};
    std::snprintf(level, sizeof(level), " / Feature Level 0x%04X", static_cast<unsigned>(selected.featureLevel));
    Logger::Info("DirectX12 GPU: " + ConvertString(selected.adapter.Description) + level);
}

void DirectXCommon::InitializeCommand() {
    if (!device_) RequireGraphics(E_POINTER, "InitializeCommand device");
    RequireGraphics(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_)), "CreateCommandAllocator");
    RequireGraphics(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_)), "CreateCommandList");
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    RequireGraphics(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)), "CreateCommandQueue");
}

void DirectXCommon::InitializeSwapChain(WinApp* winApp) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainWidth_ = (std::max)(winApp->GetClientWidth(), 1u);
    swapChainHeight_ = (std::max)(winApp->GetClientHeight(), 1u);
    swapChainDesc.Width = swapChainWidth_; swapChainDesc.Height = swapChainHeight_;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
}

void DirectXCommon::InitializeRenderTargetView() {
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 繧ｹ繝ｯ繝・・繝√ぉ繝ｼ繝ｳ逕ｨ2蛟・+ 繝薙Η繝ｼ繝昴・繝育畑1蛟・+ 霑ｽ蜉繧ｨ繝輔ぉ繧ｯ繝育ｭ臥畑縺ｫ螟壹ａ縺ｫ遒ｺ菫・64蛟・
    rtvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, false);
    for (uint32_t i = 0; i < 2; ++i) {
        swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<size_t>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
    }
}

void DirectXCommon::InitializeDepthStencilView() {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = WinApp::kClientWidth; desc.Height = WinApp::kClientHeight;
    desc.MipLevels = 1; desc.DepthOrArraySize = 1; desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = 1; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES prop{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clear{}; clear.DepthStencil.Depth = 1.0f; clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    HRESULT hr = device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&depthBuffer_));
    assert(SUCCEEDED(hr));
    depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    dsvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeRenderTexture() {
    sceneRenderTexture_.Initialize(
        this,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTargetFormat,
        { renderTextureClearColor_[0], renderTextureClearColor_[1], renderTextureClearColor_[2], renderTextureClearColor_[3] },
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        "SceneRenderTexture");

    postEffectResultTexture_.Initialize(
        this,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTargetFormat,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        "PostEffectResultTexture");

    finalDisplayTexture_.Initialize(
        this,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTargetFormat,
        { 0.0f, 0.0f, 0.0f, 1.0f },
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        "FinalDisplayTexture");

    normalTexture_.Initialize(
        this,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kNormalTextureFormat,
        { 0.5f, 0.5f, 1.0f, 1.0f },
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        "NormalTexture");
}

void DirectXCommon::InitializeFence() {
    device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void DirectXCommon::InitializeDXCCompiler() {
    RequireGraphics(shaderCompiler_.Initialize(), "ShaderCompiler::Initialize");
    InitializeFullscreenPostEffectParameter();
}

void DirectXCommon::InitializeFullscreenPostEffectParameter() {
    constexpr size_t kConstantBufferSize = 256;
    fullscreenPostEffectParameterResource_ = CreateBufferResource(kConstantBufferSize);
    vignetteParameterResource_ = CreateBufferResource(kConstantBufferSize);
    radialBlurParameterResource_ = CreateBufferResource(kConstantBufferSize);
    dissolveParameterResource_ = CreateBufferResource(kConstantBufferSize);
    randomNoiseParameterResource_ = CreateBufferResource(kConstantBufferSize);
    hsvFilterParameterResource_ = CreateBufferResource(kConstantBufferSize);

    D3D12_RANGE readRange{ 0, 0 };
    HRESULT hr = fullscreenPostEffectParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedFullscreenPostEffectParameter_));
    assert(SUCCEEDED(hr));
    hr = vignetteParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedVignetteParameter_));
    assert(SUCCEEDED(hr));
    hr = radialBlurParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedRadialBlurParameter_));
    assert(SUCCEEDED(hr));
    hr = dissolveParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedDissolveParameter_));
    assert(SUCCEEDED(hr));
    hr = randomNoiseParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedRandomNoiseParameter_));
    assert(SUCCEEDED(hr));
    hr = hsvFilterParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedHSVFilterParameter_));
    assert(SUCCEEDED(hr));

    FullscreenPostEffectParameter parameter{};
    SetFullscreenPostEffectParameter(parameter);
    VignetteParamForGPU vignetteParameter{};
    SetVignetteParameter(vignetteParameter);
    RadialBlurParamForGPU radialBlurParameter{};
    SetRadialBlurParameter(radialBlurParameter);
    DissolveParamForGPU dissolveParameter{};
    SetDissolveParameter(dissolveParameter);
    RandomNoiseParamForGPU randomNoiseParameter{};
    SetRandomNoiseParameter(randomNoiseParameter);
    HSVFilterParamForGPU hsvFilterParameter{};
    SetHSVFilterParameter(hsvFilterParameter);
}

void DirectXCommon::ResizeSwapChainIfNeeded() {
    if (!winApp_ || !swapChain_) {
        return;
    }

    const uint32_t clientWidth = winApp_->GetClientWidth();
    const uint32_t clientHeight = winApp_->GetClientHeight();
    if (clientWidth == 0 || clientHeight == 0) {
        return;
    }
    if (clientWidth == swapChainWidth_ && clientHeight == swapChainHeight_) {
        return;
    }

    commandList_->Close();
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    for (ComPtr<ID3D12Resource>& backBuffer : backBuffers_) {
        backBuffer.Reset();
    }

    HRESULT hr = swapChain_->ResizeBuffers(
        static_cast<UINT>(backBuffers_.size()),
        clientWidth,
        clientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0);
    assert(SUCCEEDED(hr));

    swapChainWidth_ = clientWidth;
    swapChainHeight_ = clientHeight;

    for (uint32_t i = 0; i < backBuffers_.size(); ++i) {
        swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<size_t>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
    }

    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);
}

void DirectXCommon::SetFullscreenPostEffectParameter(const FullscreenPostEffectParameter& parameter) {
    if (!mappedFullscreenPostEffectParameter_) {
        return;
    }

    mappedFullscreenPostEffectParameter_->grayscaleIntensity = std::clamp(parameter.grayscaleIntensity, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->sepiaIntensity = std::clamp(parameter.sepiaIntensity, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->blurStrength = std::clamp(parameter.blurStrength, 0.0f, 16.0f);
    mappedFullscreenPostEffectParameter_->padding = 0.0f;
    mappedFullscreenPostEffectParameter_->bloomThreshold = std::clamp(parameter.bloomThreshold, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->bloomIntensity = std::clamp(parameter.bloomIntensity, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->bloomRadius = std::clamp(parameter.bloomRadius, 0.0f, 32.0f);
    mappedFullscreenPostEffectParameter_->bloomSoftKnee = std::clamp(parameter.bloomSoftKnee, 0.001f, 1.0f);
    mappedFullscreenPostEffectParameter_->outlineThreshold = std::clamp(parameter.outlineThreshold, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->outlineIntensity = std::clamp(parameter.outlineIntensity, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->outlineThickness = std::clamp(parameter.outlineThickness, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->outlinePadding = 0.0f;
    mappedFullscreenPostEffectParameter_->depthOutlineThreshold = std::clamp(parameter.depthOutlineThreshold, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->depthOutlineIntensity = std::clamp(parameter.depthOutlineIntensity, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->depthOutlineThickness = std::clamp(parameter.depthOutlineThickness, 1.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->depthOutlinePadding = 0.0f;
    mappedFullscreenPostEffectParameter_->depthOutlineNearClip = std::clamp(parameter.depthOutlineNearClip, 0.0001f, 100000.0f);
    mappedFullscreenPostEffectParameter_->depthOutlineFarClip = std::clamp(
        parameter.depthOutlineFarClip,
        mappedFullscreenPostEffectParameter_->depthOutlineNearClip + 0.0001f,
        1000000.0f);
    mappedFullscreenPostEffectParameter_->depthOutlineLinearize = parameter.depthOutlineLinearize != 0.0f ? 1.0f : 0.0f;
    mappedFullscreenPostEffectParameter_->depthOutlineLinearPadding = 0.0f;
    mappedFullscreenPostEffectParameter_->normalOutlineThreshold = std::clamp(parameter.normalOutlineThreshold, 0.0f, 4.0f);
    mappedFullscreenPostEffectParameter_->normalOutlineIntensity = std::clamp(parameter.normalOutlineIntensity, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->normalOutlineThickness = std::clamp(parameter.normalOutlineThickness, 1.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->normalOutlinePadding = 0.0f;
    mappedFullscreenPostEffectParameter_->gaussianSigma = std::clamp(parameter.gaussianSigma, 0.1f, 4.0f);
    mappedFullscreenPostEffectParameter_->gaussianPadding[0] = 0.0f;
    mappedFullscreenPostEffectParameter_->gaussianPadding[1] = 0.0f;
    mappedFullscreenPostEffectParameter_->gaussianPadding[2] = 0.0f;
}

void DirectXCommon::SetVignetteParameter(const VignetteParamForGPU& parameter) {
    if (!mappedVignetteParameter_) {
        return;
    }

    mappedVignetteParameter_->scale = std::clamp(parameter.scale, 0.0f, 64.0f);
    mappedVignetteParameter_->power = std::clamp(parameter.power, 0.01f, 8.0f);
    mappedVignetteParameter_->intensity = std::clamp(parameter.intensity, 0.0f, 1.0f);
    mappedVignetteParameter_->padding = 0.0f;
}

void DirectXCommon::SetRadialBlurParameter(const RadialBlurParamForGPU& parameter) {
    if (!mappedRadialBlurParameter_) {
        return;
    }

    mappedRadialBlurParameter_->center.x = std::clamp(parameter.center.x, 0.0f, 1.0f);
    mappedRadialBlurParameter_->center.y = std::clamp(parameter.center.y, 0.0f, 1.0f);
    mappedRadialBlurParameter_->blurWidth = std::clamp(parameter.blurWidth, 0.0f, 0.1f);
    mappedRadialBlurParameter_->intensity = std::clamp(parameter.intensity, 0.0f, 1.0f);
    mappedRadialBlurParameter_->sampleCount = std::clamp(parameter.sampleCount, 1, 32);
    mappedRadialBlurParameter_->padding[0] = 0.0f;
    mappedRadialBlurParameter_->padding[1] = 0.0f;
    mappedRadialBlurParameter_->padding[2] = 0.0f;
}

void DirectXCommon::SetDissolveParameter(const DissolveParamForGPU& parameter) {
    if (!mappedDissolveParameter_) {
        return;
    }

    mappedDissolveParameter_->threshold = std::clamp(parameter.threshold, 0.0f, 1.0f);
    mappedDissolveParameter_->edgeWidth = std::clamp(parameter.edgeWidth, 0.001f, 0.2f);
    mappedDissolveParameter_->edgeIntensity = std::clamp(parameter.edgeIntensity, 0.0f, 5.0f);
    mappedDissolveParameter_->enableEdge = parameter.enableEdge != 0.0f ? 1.0f : 0.0f;
    mappedDissolveParameter_->edgeColor.x = std::clamp(parameter.edgeColor.x, 0.0f, 1.0f);
    mappedDissolveParameter_->edgeColor.y = std::clamp(parameter.edgeColor.y, 0.0f, 1.0f);
    mappedDissolveParameter_->edgeColor.z = std::clamp(parameter.edgeColor.z, 0.0f, 1.0f);
    mappedDissolveParameter_->padding0 = 0.0f;
}

void DirectXCommon::SetRandomNoiseParameter(const RandomNoiseParamForGPU& parameter) {
    if (!mappedRandomNoiseParameter_) {
        return;
    }

    mappedRandomNoiseParameter_->time = std::isfinite(parameter.time) ? parameter.time : 0.0f;
    mappedRandomNoiseParameter_->strength = std::clamp(parameter.strength, 0.0f, 1.0f);
    mappedRandomNoiseParameter_->scale = std::clamp(parameter.scale, 1.0f, 2000.0f);
    mappedRandomNoiseParameter_->mode = parameter.mode < 0.5f ? 0.0f : 1.0f;
    mappedRandomNoiseParameter_->animate = parameter.animate != 0.0f ? 1.0f : 0.0f;
    mappedRandomNoiseParameter_->padding0 = 0.0f;
    mappedRandomNoiseParameter_->padding1 = 0.0f;
    mappedRandomNoiseParameter_->padding2 = 0.0f;
}

void DirectXCommon::SetHSVFilterParameter(const HSVFilterParamForGPU& parameter) {
    if (!mappedHSVFilterParameter_) {
        return;
    }

    mappedHSVFilterParameter_->hue = std::clamp(parameter.hue, -1.0f, 1.0f);
    mappedHSVFilterParameter_->saturation = std::clamp(parameter.saturation, -1.0f, 1.0f);
    mappedHSVFilterParameter_->value = std::clamp(parameter.value, -1.0f, 1.0f);
    mappedHSVFilterParameter_->padding = 0.0f;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetCommonPostEffectParameterAddress() const {
    return fullscreenPostEffectParameterResource_
        ? fullscreenPostEffectParameterResource_->GetGPUVirtualAddress()
        : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetVignetteParameterAddress() const {
    return vignetteParameterResource_ ? vignetteParameterResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetRadialBlurParameterAddress() const {
    return radialBlurParameterResource_ ? radialBlurParameterResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetDissolveParameterAddress() const {
    return dissolveParameterResource_ ? dissolveParameterResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetRandomNoiseParameterAddress() const {
    return randomNoiseParameterResource_ ? randomNoiseParameterResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXCommon::GetHSVFilterParameterAddress() const {
    return hsvFilterParameterResource_ ? hsvFilterParameterResource_->GetGPUVirtualAddress() : 0;
}

// --- 謠冗判繝輔Ο繝ｼ邂｡逅・---

/**
 * PreDraw: 繧ｲ繝ｼ繝逕ｻ髱｢繧偵ユ繧ｯ繧ｹ繝√Ε縺ｸ謠上″霎ｼ繧貅門ｙ
 */
void DirectXCommon::PreDraw() {
    assert(sceneRenderTexture_.GetResource());
    TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionNormalTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionDepthBuffer(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sceneRenderTexture_.GetRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    sceneRenderTexture_.Clear(commandList_.Get());
    normalTexture_.Clear(commandList_.Get());
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport{ 0, 0, (FLOAT)WinApp::kClientWidth, (FLOAT)WinApp::kClientHeight, 0, 1 };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    commandList_->RSSetScissorRects(1, &scissor);
}

void DirectXCommon::SetSceneRenderTarget() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sceneRenderTexture_.GetRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
}

void DirectXCommon::SetSceneRenderTargetsWithNormal() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
    rtvHandles[0] = sceneRenderTexture_.GetRTV();
    rtvHandles[1] = normalTexture_.GetRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, &dsvHandle);
}

void DirectXCommon::RestoreRenderTextureToRenderTarget() {
    TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
}

/**
 * PostDraw: GPU縺ｸ縺ｮ蜻ｽ莉､騾∽ｿ｡縺ｨ蜷梧悄
 */
void DirectXCommon::TransitionRenderTexture(D3D12_RESOURCE_STATES stateAfter) {
    sceneRenderTexture_.Transition(commandList_.Get(), stateAfter);
}

void DirectXCommon::TransitionPostEffectResult(D3D12_RESOURCE_STATES stateAfter) {
    postEffectResultTexture_.Transition(commandList_.Get(), stateAfter);
}

void DirectXCommon::TransitionFinalDisplayTexture(D3D12_RESOURCE_STATES stateAfter) {
    finalDisplayTexture_.Transition(commandList_.Get(), stateAfter);
}

void DirectXCommon::TransitionNormalTexture(D3D12_RESOURCE_STATES stateAfter) {
    normalTexture_.Transition(commandList_.Get(), stateAfter);
}

void DirectXCommon::TransitionDepthBuffer(D3D12_RESOURCE_STATES stateAfter) {
    if (!depthBuffer_ || depthBufferState_ == stateAfter) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthBuffer_.Get();
    barrier.Transition.StateBefore = depthBufferState_;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    depthBufferState_ = stateAfter;
}

void DirectXCommon::SetFullscreenViewportAndScissor() {
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<FLOAT>(WinApp::kClientWidth), static_cast<FLOAT>(WinApp::kClientHeight), 0.0f, 1.0f };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    commandList_->RSSetScissorRects(1, &scissor);
}

void DirectXCommon::SetSwapChainViewportAndScissor() {
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<FLOAT>(swapChainWidth_), static_cast<FLOAT>(swapChainHeight_), 0.0f, 1.0f };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, static_cast<LONG>(swapChainWidth_), static_cast<LONG>(swapChainHeight_) };
    commandList_->RSSetScissorRects(1, &scissor);
}

void DirectXCommon::BeginPostEffectResultRenderTarget() {
    assert(postEffectResultTexture_.GetResource());
    TransitionPostEffectResult(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = postEffectResultTexture_.GetRTV();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    postEffectResultTexture_.Clear(commandList_.Get());
    SetFullscreenViewportAndScissor();
}

void DirectXCommon::BeginFinalDisplayRenderTarget() {
    assert(finalDisplayTexture_.GetResource());
    TransitionFinalDisplayTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = finalDisplayTexture_.GetRTV();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    finalDisplayTexture_.Clear(commandList_.Get());
    SetFullscreenViewportAndScissor();
}

void DirectXCommon::BeginSwapChainRenderTarget() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(backBufferIndex) * rtvDescriptorSize_;
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    SetSwapChainViewportAndScissor();
}

void DirectXCommon::PostDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    commandList_->Close();
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    swapChain_->Present(1, 0);
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);
}

// --- 繝ｪ繧ｽ繝ｼ繧ｹ邂｡逅・・繝ｫ繝代・ ---

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const std::wstring& profile) {
    auto bytecode = shaderCompiler_.Compile(filePath, profile);
    if (!bytecode) Logger::Error("Shader path: " + ConvertString(filePath));
    return bytecode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
    loading::Scope timing(loading::Stage::BufferAllocation);
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = sizeInBytes; desc.Height = 1; desc.DepthOrArraySize = 1;
    desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_STATES initialState) {
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{ type, numDescriptors, shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
    ComPtr<ID3D12DescriptorHeap> heap; device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    return heap;
}

uint32_t DirectXCommon::AllocateRTV() {
    uint32_t index = nextRtvIndex_;
    nextRtvIndex_++;
    return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<size_t>(index) * rtvDescriptorSize_;
    return handle;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float* clearColor) {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.DepthOrArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    if (clearColor) {
        clearValue.Color[0] = clearColor[0];
        clearValue.Color[1] = clearColor[1];
        clearValue.Color[2] = clearColor[2];
        clearValue.Color[3] = clearColor[3];
    } else {
        clearValue.Color[0] = 0.0f; clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f; clearValue.Color[3] = 1.0f;
    }

    ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue, IID_PPV_ARGS(&resource));
    return resource;
}

