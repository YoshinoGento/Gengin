#include "farm/system/FarmContestAudioSystem.h"

bool FarmContestAudioSystem::Initialize(Audio* audio, const std::string& directory) {
    Finalize();
    if (!audio) return false;
    audio_ = audio;
    constexpr std::array<const char*,3> files{"assess.wav", "passed.wav", "failed.wav"};
    for (std::size_t i=0; i<files.size(); ++i) {
        clips_[i] = audio_->LoadAudio(directory+"/"+files[i]);
        if (!clips_[i]) { Finalize(); return false; }
    }
    return true;
}
float FarmContestAudioSystem::VolumeFor(FarmContestPresentationSystem::Cue cue, int percent) noexcept {
    using Cue = FarmContestPresentationSystem::Cue;
    constexpr float kAssessmentGain = .14f, kVerdictGain = .22f;
    float gain = 0;
    switch (cue) {
    case Cue::Assess: case Cue::Recorded: gain = kAssessmentGain; break;
    case Cue::Passed: case Cue::Failed: gain = kVerdictGain; break;
    default: return 0;
    }
    return std::clamp(percent,0,100)*.01f*gain;
}
void FarmContestAudioSystem::Begin(int contestPercent) noexcept {
    Stop();
    volumePercent_ = std::clamp(contestPercent,0,100);
}
void FarmContestAudioSystem::Update(FarmContestPresentationSystem::Cue cue, bool active, bool focused) {
    if (!active || !focused) { Stop(); return; }
    const float gain = VolumeFor(cue,volumePercent_);
    if (!audio_ || gain <= 0) return;
    using Cue = FarmContestPresentationSystem::Cue;
    std::size_t index = 0;
    switch (cue) {
    case Cue::Assess: case Cue::Recorded: break;
    case Cue::Passed: index=1; break;
    case Cue::Failed: index=2; break;
    default: return;
    }
    // One owned voice: skip never stacks an assessment with a verdict.
    Stop();
    Audio::PlaySettings play;
    play.volume = gain;
    voice_ = audio_->Play(clips_[index],play);
}
void FarmContestAudioSystem::Stop() noexcept {
    if (audio_ && voice_) (void)audio_->StopVoice(voice_);
    voice_ = {};
}
void FarmContestAudioSystem::Finalize() noexcept {
    Stop();
    if (audio_) for (auto clip : clips_) if (clip) (void)audio_->UnloadAudio(clip);
    clips_ = {}; audio_ = nullptr; volumePercent_ = 0;
}
