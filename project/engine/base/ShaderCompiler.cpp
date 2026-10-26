#include "ShaderCompiler.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <string_view>
#include <utility>

#pragma comment(lib, "dxcompiler.lib")
using Microsoft::WRL::ComPtr;
namespace graphics {
namespace {
bool SameBytes(IDxcBlob* a, IDxcBlob* b) noexcept {
    return a && b && a->GetBufferSize() == b->GetBufferSize() &&
        (a->GetBufferSize() == 0 || std::memcmp(a->GetBufferPointer(), b->GetBufferPointer(), a->GetBufferSize()) == 0);
}
bool HasVolatilePreprocessing(IDxcBlob* source) noexcept {
    const std::string_view text(static_cast<const char*>(source->GetBufferPointer()), source->GetBufferSize());
    for (const std::string_view token : {"__DATE__", "__TIME__", "__TIMESTAMP__", "__has_include"})
        if (text.find(token) != std::string_view::npos) return true;
    return false;
}
}
class ShaderCompiler::IncludeRecorder final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDxcIncludeHandler> {
public:
    IncludeRecorder(IDxcIncludeHandler* delegate, const std::filesystem::path& directory)
        : delegate_(delegate), directory_(directory) {}
    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob** result) override {
        if (!filename || !result) return E_POINTER;
        *result = nullptr;
        const HRESULT hr = delegate_->LoadSource(filename, result);
        if (FAILED(hr) || !*result) { cacheable = false; return FAILED(hr) ? hr : E_FAIL; }
        try {
            std::error_code error;
            const auto resolved = std::filesystem::canonical(filename, error);
            const auto path = std::filesystem::absolute(filename).lexically_normal();
            // External include search chains may gain a higher-priority file; do not cache them.
            if (error || resolved != path || resolved.parent_path() != directory_ || dependencies.size() >= kMaximumDependencies) {
                cacheable = false;
            } else {
                if (std::none_of(dependencies.begin(), dependencies.end(),
                    [&](const Dependency& dependency) { return dependency.path == path; }))
                    dependencies.push_back({path, ComPtr<IDxcBlob>(*result)});
            }
        } catch (...) { cacheable = false; }
        return hr;
    }
    bool cacheable = true;
    std::vector<Dependency> dependencies;
private:
    ComPtr<IDxcIncludeHandler> delegate_;
    std::filesystem::path directory_;
};

HRESULT ShaderCompiler::Initialize() {
    ClearCache(); stats_ = {};
    includes_.Reset(); compiler_.Reset(); utils_.Reset();
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
    if (FAILED(hr)) return hr;
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
    if (FAILED(hr)) return hr;
    return utils_->CreateDefaultIncludeHandler(&includes_);
}
void ShaderCompiler::ClearCache() noexcept {
    cache_.clear(); stats_.entries = 0; stats_.retainedBytes = 0;
}
void ShaderCompiler::Erase(std::size_t index) noexcept {
    stats_.retainedBytes -= cache_[index].bytes;
    cache_.erase(cache_.begin() + static_cast<std::ptrdiff_t>(index));
    stats_.entries = cache_.size();
}
void ShaderCompiler::Remember(Entry entry) {
    entry.bytes = entry.bytecode->GetBufferSize();
    if (entry.bytes > kMaximumRetainedBytes) return;
    for (const auto& dependency : entry.dependencies) {
        if (HasVolatilePreprocessing(dependency.contents.Get())) return;
        const auto bytes = dependency.contents->GetBufferSize();
        if (bytes > kMaximumRetainedBytes - entry.bytes) return;
        entry.bytes += bytes;
    }
    while (!cache_.empty() && (cache_.size() >= kMaximumEntries ||
        stats_.retainedBytes > kMaximumRetainedBytes - entry.bytes)) Erase(0);
    cache_.push_back(std::move(entry));
    stats_.retainedBytes += cache_.back().bytes;
    stats_.entries = cache_.size();
}
ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring& filePath, const std::wstring& profile, bool useCache) {
    ++stats_.requests;
    if (!utils_ || !compiler_ || !includes_ || filePath.empty() || profile.empty()) {
        Logger::Error("ShaderCompiler: unavailable compiler or empty path/profile.");
        return nullptr;
    }
    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(filePath, error);
    if (error) { Logger::Error("ShaderCompiler: invalid source path."); return nullptr; }
    ComPtr<IDxcBlobEncoding> source;
    const HRESULT loaded = utils_->LoadFile(path.c_str(), nullptr, &source);
    for (std::size_t i = 0; i < cache_.size(); ++i) {
        const auto& entry = cache_[i];
        if (entry.path != path || entry.profile != profile) continue;
        bool unchanged = SUCCEEDED(loaded) && SameBytes(source.Get(), entry.dependencies.front().contents.Get());
        if (unchanged && useCache) for (std::size_t d = 1; d < entry.dependencies.size(); ++d) {
            ComPtr<IDxcBlobEncoding> current;
            const auto resolved = std::filesystem::canonical(entry.dependencies[d].path, error);
            if (error || resolved != entry.dependencies[d].path ||
                FAILED(utils_->LoadFile(entry.dependencies[d].path.c_str(), nullptr, &current)) ||
                !SameBytes(current.Get(), entry.dependencies[d].contents.Get())) { unchanged = false; break; }
        }
        if (unchanged && useCache) { ++stats_.hits; return entry.bytecode; }
        Erase(i); ++stats_.invalidations;
        break;
    }
    if (FAILED(loaded) || !source || !source->GetBufferSize()) {
        Logger::Error("ShaderCompiler: source file missing or empty."); return nullptr;
    }
    const std::wstring includePath = L"-I" + path.parent_path().wstring();
    LPCWSTR arguments[]{path.c_str(), L"-E", L"main", L"-T", profile.c_str(),
        L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr", includePath.c_str()};
    auto recorder = Microsoft::WRL::Make<IncludeRecorder>(includes_.Get(), path.parent_path());
    if (!recorder) { Logger::Error("ShaderCompiler: include recorder allocation failed."); return nullptr; }
    DxcBuffer buffer{source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8};
    ComPtr<IDxcResult> result;
    const auto start = std::chrono::steady_clock::now();
    ++stats_.compiles;
    const HRESULT requested = compiler_->Compile(&buffer, arguments, static_cast<UINT32>(std::size(arguments)),
        recorder.Get(), IID_PPV_ARGS(&result));
    stats_.compileMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    HRESULT status = E_FAIL;
    if (FAILED(requested) || !result || FAILED(result->GetStatus(&status)) || FAILED(status)) {
        ComPtr<IDxcBlobUtf8> errors;
        if (result) result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        Logger::Error(errors && errors->GetStringLength() ? errors->GetStringPointer() : "ShaderCompiler: DXC compilation failed.");
        return nullptr;
    }
    // Preserve the engine's existing strict diagnostics policy.
    ComPtr<IDxcBlobUtf8> diagnostics;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&diagnostics), nullptr);
    if (diagnostics && diagnostics->GetStringLength()) { Logger::Error(diagnostics->GetStringPointer()); return nullptr; }
    ComPtr<IDxcBlob> bytecode;
    if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr)) || !bytecode || !bytecode->GetBufferSize()) {
        Logger::Error("ShaderCompiler: DXC produced no bytecode."); return nullptr;
    }
    if (useCache && recorder->cacheable) {
        try {
            Entry entry{path, profile, {}, bytecode};
            entry.dependencies.reserve(1 + recorder->dependencies.size());
            entry.dependencies.push_back({path, source});
            for (auto& dependency : recorder->dependencies) entry.dependencies.push_back(std::move(dependency));
            Remember(std::move(entry));
        } catch (const std::bad_alloc&) { Logger::Warning("ShaderCompiler: cache allocation skipped; bytecode is valid."); }
    }
    return bytecode;
}
}
