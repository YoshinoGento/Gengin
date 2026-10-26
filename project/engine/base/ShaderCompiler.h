#pragma once
#include <Windows.h>
#include <dxcapi.h>
#include <wrl.h>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace graphics {
// Main-thread compiler. Cached blobs are CPU-owned; PSO/Descriptor/Fence ownership stays with renderers.
class ShaderCompiler final {
public:
    struct Stats {
        std::size_t requests = 0, hits = 0, compiles = 0, invalidations = 0;
        std::size_t entries = 0, retainedBytes = 0;
        double compileMs = 0;
    };
    static constexpr std::size_t kMaximumEntries = 64;
    static constexpr std::size_t kMaximumRetainedBytes = 16 * 1024 * 1024;
    static constexpr std::size_t kMaximumDependencies = 64;
    ShaderCompiler() = default;
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    HRESULT Initialize();
    Microsoft::WRL::ComPtr<IDxcBlob> Compile(const std::wstring& filePath,
        const std::wstring& profile, bool useCache = true);
    void ClearCache() noexcept;
    const Stats& GetStats() const noexcept { return stats_; }
private:
    struct Dependency {
        std::filesystem::path path;
        Microsoft::WRL::ComPtr<IDxcBlob> contents;
    };
    struct Entry {
        std::filesystem::path path;
        std::wstring profile;
        std::vector<Dependency> dependencies;
        Microsoft::WRL::ComPtr<IDxcBlob> bytecode;
        std::size_t bytes = 0;
    };
    class IncludeRecorder;
    void Erase(std::size_t index) noexcept;
    void Remember(Entry entry);
    Microsoft::WRL::ComPtr<IDxcUtils> utils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includes_;
    std::vector<Entry> cache_;
    Stats stats_{};
};
}
