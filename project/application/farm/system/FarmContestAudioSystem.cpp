#include "farm/system/FarmContestAudioSystem.h"
#include "base/Logger.h"

bool FarmContestAudioSystem::Initialize(Audio* audio, const std::string& directory) {
    Finalize();
    if (!audio) return false;
    Logger::Info(farm::cheerassets::kAssetContract);
    audio_ = audio;
    constexpr std::array<const char*,5> files{"assess.wav", "passed.wav", "failed.wav", "clear.wav", "drumroll.wav"};
    for (std::size_t i=0; i<files.size(); ++i) {
        clips_[i] = audio_->LoadAudio(directory+"/"+files[i]);
        if (!clips_[i]) { Finalize(); return false; }
    }
    for (std::size_t i=0; i<cheerClips_.size(); ++i) {
        cheerClips_[i] = audio_->LoadAudio(directory+"/"+farm::cheerassets::kClips[i].file);
        if (!cheerClips_[i]) {
            // Speech is optional: retain the original cues and readable captions on an incomplete install.
            for (auto clip : cheerClips_) if (clip) (void)audio_->UnloadAudio(clip);
            cheerClips_ = {};
            Logger::Warning("Contest spectator speech is incomplete; using caption-only presentation.");
            return true;
        }
    }
    cheersReady_ = true;
    return true;
}
float FarmContestAudioSystem::VolumeFor(FarmContestPresentationSystem::Cue cue, int percent) noexcept {
    using Cue = FarmContestPresentationSystem::Cue;
    constexpr float kAssessmentGain = .14f, kVerdictGain = .22f, kDrumrollGain = .18f;
    float gain = 0;
    switch (cue) {
    case Cue::Assess: case Cue::Recorded: gain = kAssessmentGain; break;
    case Cue::Drumroll: gain = kDrumrollGain; break;
    case Cue::Passed: case Cue::Failed: case Cue::Clear: gain = kVerdictGain; break;
    default: return 0;
    }
    return std::clamp(percent,0,100)*.01f*gain;
}
void FarmContestAudioSystem::Begin(int contestPercent) noexcept {
    Stop();
    volumePercent_ = std::clamp(contestPercent,0,100);
}
void FarmContestAudioSystem::Update(FarmContestPresentationSystem::Cue cue, bool active, bool focused,
    const FarmContestCheerSystem::Frame& cheer) {
    if (!active || cue==FarmContestPresentationSystem::Cue::Silence) { Stop(); return; }
    if (!focused) {
        if (audio_ && voice_ && !paused_) paused_=audio_->PauseVoice(voice_);
        if (audio_ && cheerVoice_ && !cheerPaused_) cheerPaused_=audio_->PauseVoice(cheerVoice_);
        return;
    }
    if (paused_) {
        if (audio_ && voice_) (void)audio_->ResumeVoice(voice_);
        paused_=false;
    }
    if (cheerPaused_) {
        if (audio_ && cheerVoice_) (void)audio_->ResumeVoice(cheerVoice_);
        cheerPaused_=false;
    }
    if (!audio_) return;
    const float gain = VolumeFor(cue,volumePercent_);
    using Cue = FarmContestPresentationSystem::Cue;
    std::size_t index = 0;
    switch (cue) {
    case Cue::Assess: case Cue::Recorded: break;
    case Cue::Passed: index=1; break;
    case Cue::Failed: index=2; break;
    case Cue::Clear: index=3; break;
    case Cue::Drumroll: index=4; break;
    default: break;
    }
    if (gain > 0) {
        StopPhase();
        phaseGain_ = gain;
        Audio::PlaySettings play;
        play.volume = gain;
        voice_ = audio_->Play(clips_[index],play);
    }
    const int speech = FarmContestCheerSystem::ClipIndex(cheer.pair,cheer.side);
    if (speech < 0 || !cheer.voiceReady) StopCheer();
    else if (speech != lastCheer_) {
        StopCheer();
        lastCheer_ = speech;
        if (cheersReady_ && volumePercent_ > 0) {
            constexpr float kSpeechGain = .60f;
            Audio::PlaySettings play;
            play.volume = volumePercent_*.01f*kSpeechGain;
            cheerVoice_ = audio_->Play(cheerClips_[speech],play);
        }
    }
    constexpr float kSpeechDuck = .68f;
    const float duck = cheersReady_ && std::isfinite(cheer.ducking) ? std::clamp(cheer.ducking,0.f,1.f) : 0;
    if (voice_) (void)audio_->SetVoiceVolume(voice_,phaseGain_*(1-kSpeechDuck*duck));
}
void FarmContestAudioSystem::StopPhase() noexcept {
    if (audio_ && voice_) (void)audio_->StopVoice(voice_);
    voice_ = {};
    paused_ = false;
    phaseGain_ = 0;
}
void FarmContestAudioSystem::StopCheer() noexcept {
    if (audio_ && cheerVoice_) (void)audio_->StopVoice(cheerVoice_);
    cheerVoice_ = {};
    cheerPaused_ = false;
}
void FarmContestAudioSystem::Stop() noexcept {
    StopPhase(); StopCheer(); lastCheer_ = -1;
}
void FarmContestAudioSystem::Finalize() noexcept {
    Stop();
    if (audio_) for (auto clip : clips_) if (clip) (void)audio_->UnloadAudio(clip);
    if (audio_) for (auto clip : cheerClips_) if (clip) (void)audio_->UnloadAudio(clip);
    cheerClips_ = {}; cheersReady_ = false;
    clips_ = {}; audio_ = nullptr; volumePercent_ = 0;
}
