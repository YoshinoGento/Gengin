#include "farm/system/FarmPestAudioSystem.h"

bool FarmPestAudioSystem::Initialize(Audio* audio) {
    Finalize();
    if (!audio) return false;
    audio_ = audio;
    clip_ = audio_->LoadAudio("Resources/farm/pests/audio/cured.wav");
    if (!clip_) { Finalize(); return false; }
    return true;
}
void FarmPestAudioSystem::PlayCured(int effectPercent) {
    Stop();
    const float volume = VolumeFor(effectPercent);
    if (!audio_ || !clip_ || volume <= 0) return;
    Audio::PlaySettings settings;
    settings.volume = volume;
    voice_ = audio_->Play(clip_,settings);
}
void FarmPestAudioSystem::Stop() noexcept {
    if (audio_ && voice_) (void)audio_->StopVoice(voice_);
    voice_ = {};
}
void FarmPestAudioSystem::Finalize() noexcept {
    Stop();
    if (audio_ && clip_) (void)audio_->UnloadAudio(clip_);
    clip_ = {}; audio_ = nullptr;
}
