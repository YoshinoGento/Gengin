#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace loading {
enum class Stage : std::size_t {
    ModelLookup, ModelImport, ModelBuffers, BufferAllocation,
    TextureDecode, TextureMips, TextureResource, TextureUploadCpu, TextureSubmitWait,
    SkyGeometry, SkyShaders, SkyRootSignature, SkyPipeline,
    AudioDecode, AudioVoiceSetup, Count
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Stage::Count)> kStageNames{
    "model.lookup", "model.import", "model.buffers", "buffer.allocation",
    "texture.decode", "texture.mips", "texture.resource", "texture.uploadCpu", "texture.submitWait",
    "sky.geometry", "sky.shaders", "sky.rootSignature", "sky.pipeline",
    "audio.decode", "audio.voiceSetup"
};
struct Counter { std::uint64_t calls = 0; double totalMs = 0, maximumMs = 0; };
using Counters = std::array<Counter, kStageNames.size()>;
// Owner-thread diagnostics only. Nested scopes overlap; these totals must not be summed.
inline thread_local bool enabled = false;
inline thread_local Counters counters{};
inline void Reset() noexcept { counters = {}; }
class Scope final {
public:
    explicit Scope(Stage stage) noexcept : stage_(stage), active_(enabled && static_cast<std::size_t>(stage) < counters.size()) {
        if (active_) start_ = Clock::now();
    }
    ~Scope() { Stop(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    void Stop() noexcept {
        if (!active_) return;
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
        auto& counter = counters[static_cast<std::size_t>(stage_)];
        ++counter.calls;
        counter.totalMs += ms;
        if (ms > counter.maximumMs) counter.maximumMs = ms;
        active_ = false;
    }
private:
    using Clock = std::chrono::steady_clock;
    Stage stage_;
    bool active_;
    Clock::time_point start_{};
};
}
