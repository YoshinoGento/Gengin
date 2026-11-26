#pragma once
#include "audio/Audio.h"
#include "farm/system/FarmFieldCue.h"
#include "farm/system/FarmToolSystem.h"
#include <array>

namespace farm {
class FarmFieldAudioSystem final {
public:
    FarmFieldAudioSystem() = default;
    ~FarmFieldAudioSystem() { Finalize(); }
    FarmFieldAudioSystem(const FarmFieldAudioSystem&) = delete;
    FarmFieldAudioSystem& operator=(const FarmFieldAudioSystem&) = delete;
    bool Initialize(Audio* audio);
    FieldCues Update(FieldCues cues, bool audible, float dt, int effects, int water,
        FarmTool tool, int seed);
    void Stop() noexcept;
    void Finalize() noexcept;
private:
    struct Slot { AudioClipHandle clip{}; AudioVoiceHandle voice{}; float wait{}; };
    std::array<Slot,static_cast<std::size_t>(FieldCue::Count)> slots_{};
    Audio* audio_{}; // Framework outlives scene audio; all calls are on its owner thread.
    FarmTool tool_{};
    int seed_{};
    bool tracking_{};
};
}
