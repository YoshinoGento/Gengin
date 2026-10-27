#pragma once
#include "audio/Audio.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include <array>

// Session-owned private clips; Framework Audio must outlive this System.
class FarmContestAudioSystem final {
public:
    ~FarmContestAudioSystem() { Finalize(); }
    FarmContestAudioSystem() = default;
    FarmContestAudioSystem(const FarmContestAudioSystem&) = delete;
    FarmContestAudioSystem& operator=(const FarmContestAudioSystem&) = delete;
    bool Initialize(Audio* audio, const std::string& directory = "Resources/farm/contest/audio");
    void Begin(int contestPercent) noexcept;
    [[nodiscard]] static float VolumeFor(FarmContestPresentationSystem::Cue cue, int percent) noexcept;
    void Update(FarmContestPresentationSystem::Cue cue, bool active, bool focused);
    void Stop() noexcept;
    void Finalize() noexcept;
private:
    Audio* audio_ = nullptr;
    std::array<AudioClipHandle,3> clips_{};
    AudioVoiceHandle voice_{};
    int volumePercent_ = 0;
};
