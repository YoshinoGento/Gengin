#pragma once
#include "audio/Audio.h"
#include <algorithm>

// Owns a private clip and at most one voice; Framework Audio outlives this System.
class FarmPestAudioSystem final {
public:
    FarmPestAudioSystem() = default;
    ~FarmPestAudioSystem() { Finalize(); }
    FarmPestAudioSystem(const FarmPestAudioSystem&) = delete;
    FarmPestAudioSystem& operator=(const FarmPestAudioSystem&) = delete;
    bool Initialize(Audio* audio);
    void PlayCured(int effectPercent);
    void Stop() noexcept;
    void Finalize() noexcept;
    static float VolumeFor(int percent) noexcept { return std::clamp(percent,0,100)*.01f*.18f; }
private:
    Audio* audio_ = nullptr;
    AudioClipHandle clip_{};
    AudioVoiceHandle voice_{};
};
