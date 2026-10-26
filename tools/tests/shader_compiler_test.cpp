#include "base/ShaderCompiler.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Write(const fs::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    Require(file.good(), "fixture write");
}
bool Equal(IDxcBlob* a, IDxcBlob* b) {
    return a && b && a->GetBufferSize() == b->GetBufferSize() &&
        std::memcmp(a->GetBufferPointer(), b->GetBufferPointer(), a->GetBufferSize()) == 0;
}
int main() try {
    graphics::ShaderCompiler compiler;
    Require(!compiler.Compile(L"missing", L"ps_6_0"), "uninitialized must fail");
    Require(SUCCEEDED(compiler.Initialize()), "initialize DXC");
    Require(!compiler.Compile(L"", L"ps_6_0"), "empty path");
    const auto root = fs::absolute(fs::path("generated/codex_checks") /
        ("shader_cache_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
    fs::create_directories(root);
    const auto source = root / "main.PS.hlsl", common = root / "common.hlsli", leaf = root / "leaf.hlsli";
    constexpr std::string_view body = "#include \"common.hlsli\"\nfloat4 main():SV_Target{return float4(Shade(),0,0,1);}\n";
    Write(leaf, "#define VALUE 0.25\n");
    Write(common, "#include \"leaf.hlsli\"\nfloat Shade(){return VALUE;}\n");
    Write(source, body);
    auto first = compiler.Compile(source.wstring(), L"ps_6_0");
    Require(first && compiler.GetStats().entries == 1, "cache nested includes");
    auto hit = compiler.Compile(fs::relative(source).wstring(), L"ps_6_0");
    Require(hit.Get() == first.Get() && compiler.GetStats().hits == 1, "normalized path cache hit");
    auto fresh = compiler.Compile(source.wstring(), L"ps_6_0", false);
    Require(Equal(first.Get(), fresh.Get()), "cached vs uncached byte identity");
    Require(compiler.Compile(source.wstring(), L"ps_6_0") != nullptr, "refill");
    const auto leafTime = fs::last_write_time(leaf);
    Write(leaf, "#define VALUE 0.75\n"); fs::last_write_time(leaf, leafTime);
    const auto before = compiler.GetStats().compiles;
    auto changed = compiler.Compile(source.wstring(), L"ps_6_0");
    Require(changed && !Equal(changed.Get(), first.Get()) && compiler.GetStats().compiles == before + 1,
        "same-size/same-time nested include content change");
    const auto sourceTime = fs::last_write_time(source);
    auto alternateBody = std::string(body);
    alternateBody.replace(alternateBody.find(",0,0,1"), 6, ",1,0,1");
    Write(source, alternateBody); fs::last_write_time(source, sourceTime);
    auto changedRoot = compiler.Compile(source.wstring(), L"ps_6_0");
    Require(changedRoot && !Equal(changedRoot.Get(), changed.Get()), "root content invalidation");
    auto otherProfile = compiler.Compile(source.wstring(), L"ps_6_1");
    Require(otherProfile && compiler.GetStats().entries == 2, "profile separation");
    fs::remove(leaf);
    Require(!compiler.Compile(source.wstring(), L"ps_6_0"), "missing include never returns stale code");
    Write(leaf, "#define VALUE 0.25\n");
    Require(compiler.Compile(source.wstring(), L"ps_6_0") != nullptr, "recover include");
    Write(source, "this is not a shader");
    const auto failedBefore = compiler.GetStats().compiles;
    Require(!compiler.Compile(source.wstring(), L"ps_6_0") && !compiler.Compile(source.wstring(), L"ps_6_0"), "failure not cached");
    Require(compiler.GetStats().compiles == failedBefore + 2, "failed retries actually compiled");
    fs::remove(source);
    Require(!compiler.Compile(source.wstring(), L"ps_6_1"), "deleted root never returns stale code");
    compiler.ClearCache();
    Require(!compiler.GetStats().entries && !compiler.GetStats().retainedBytes && first->GetBufferSize() > 0,
        "clear releases cache ownership but caller blob survives");

    fs::create_directory(root / "nested");
    const auto external = root / "nested" / "external.PS.hlsl";
    Write(external, "#include \"../leaf.hlsli\"\nfloat4 main():SV_Target{return VALUE;}\n");
    const auto externalBefore = compiler.GetStats().compiles;
    Require(compiler.Compile(external.wstring(), L"ps_6_0") != nullptr &&
        compiler.Compile(external.wstring(), L"ps_6_0") != nullptr, "external include compile");
    Require(!compiler.GetStats().entries && compiler.GetStats().compiles == externalBefore + 2,
        "external search chain bypasses cache");

    constexpr std::string_view simple = "float4 main():SV_Target{return float4(1,0,0,1);}\n";
    for (std::size_t i = 0; i < graphics::ShaderCompiler::kMaximumEntries + 2; ++i) {
        const auto path = root / ("capacity_" + std::to_string(i) + ".hlsl");
        Write(path, simple);
        Require(compiler.Compile(path.wstring(), L"ps_6_0") != nullptr, "capacity compile");
        Require(compiler.GetStats().entries <= graphics::ShaderCompiler::kMaximumEntries &&
            compiler.GetStats().retainedBytes <= graphics::ShaderCompiler::kMaximumRetainedBytes, "bounded cache");
    }
    const auto evictedBefore = compiler.GetStats().compiles;
    Require(compiler.Compile((root / "capacity_0.hlsl").wstring(), L"ps_6_0") != nullptr &&
        compiler.GetStats().compiles == evictedBefore + 1, "oldest entry evicted");
    compiler.ClearCache();
    std::string large = "/*" + std::string(graphics::ShaderCompiler::kMaximumRetainedBytes, 'x') + "*/\n" + std::string(simple);
    Write(source, large);
    Require(compiler.Compile(source.wstring(), L"ps_6_0") != nullptr && !compiler.GetStats().entries,
        "oversized source compiles without retention");
    Write(source, simple);

    compiler.ClearCache();
    Write(source, std::string(simple) + "// __has_include environment-dependent preprocessing\n");
    Require(compiler.Compile(source.wstring(), L"ps_6_0") != nullptr && !compiler.GetStats().entries,
        "volatile preprocessing conservatively bypasses cache");
    std::string manyIncludes;
    for (std::size_t i = 0; i < graphics::ShaderCompiler::kMaximumDependencies + 1; ++i) {
        const std::string name = "dependency_" + std::to_string(i) + ".hlsli";
        Write(root / name, "// dependency\n");
        manyIncludes += "#include \"" + name + "\"\n";
    }
    Write(source, manyIncludes + std::string(simple));
    Require(compiler.Compile(source.wstring(), L"ps_6_0") != nullptr && !compiler.GetStats().entries,
        "dependency cap bypasses cache without rejecting valid shader");

    std::size_t titleShaders = 0;
    for (const auto& item : fs::directory_iterator("project/Resources/shader")) {
        const auto name = item.path().filename().string();
        if (!name.starts_with("Title") || !name.ends_with(".hlsl")) continue;
        const auto profile = name.ends_with(".VS.hlsl") ? L"vs_6_0" : L"ps_6_0";
        auto compiled = compiler.Compile(item.path().wstring(), profile);
        const auto hits = compiler.GetStats().hits;
        auto reused = compiler.Compile(item.path().wstring(), profile);
        Require(compiled && reused.Get() == compiled.Get() && compiler.GetStats().hits == hits + 1, "actual title shader hit");
        auto uncached = compiler.Compile(item.path().wstring(), profile, false);
        Require(Equal(reused.Get(), uncached.Get()), "actual title shader bytecode parity");
        ++titleShaders;
    }
    Require(titleShaders >= 10, "title shader coverage");
    Require(SUCCEEDED(compiler.Initialize()) && !compiler.GetStats().requests && !compiler.GetStats().retainedBytes, "reinitialize resets compiler cache");
    std::cout << "PASS shader cache: nested/root content invalidation, deletion/failure/recovery, profiles, external bypass, eviction, byte budget, caller lifetime; "
        << titleShaders << " title shaders cached/uncached byte-identical\nFixtures: " << root << '\n';
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
