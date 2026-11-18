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
    void Update(FarmContestPresentationSystem::Cue cue, bool active, bool focused,
        const FarmContestCheerSystem::Frame& cheer = {});
    [[nodiscard]] bool CheersReady() const noexcept { return cheersReady_; }
    void Stop() noexcept;
    void Finalize() noexcept;
private:
    Audio* audio_ = nullptr;
    std::array<AudioClipHandle,5> clips_{};
    std::array<AudioClipHandle,farm::cheerassets::kClips.size()> cheerClips_{};
    AudioVoiceHandle voice_{};
    AudioVoiceHandle cheerVoice_{};
    bool paused_ = false;
    bool cheerPaused_ = false, cheersReady_ = false;
    int lastCheer_ = -1;
    float phaseGain_ = 0;
    void StopPhase() noexcept;
    void StopCheer() noexcept;
    int volumePercent_ = 0;
};
