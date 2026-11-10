#include "farm/system/FarmRainAudioSystem.h"

bool FarmRainAudioSystem::Initialize(Audio* audio) {
    Finalize();
    if (!audio) return false;
    audio_=audio; clip_=audio_->LoadAudio("Resources/farm/weather/audio/rain.wav");
    if (!clip_) { Finalize(); return false; }
    return true;
}
void FarmRainAudioSystem::Update(float rain, int environmentPercent, bool audible, float realSeconds) {
    const float target=VolumeFor(rain,environmentPercent);
    if (!audio_ || !clip_ || !audible || environmentPercent<=0) { Stop(); return; }
    const float dt=std::isfinite(realSeconds) ? std::clamp(realSeconds,0.0f,.1f) : 0;
    volume_+=std::clamp(target-volume_,-dt*.30f,dt*.30f);
    if (volume_<.0001f) { Stop(); return; }
    if (voice_ && !audio_->IsVoiceActive(voice_)) voice_={};
    if (!voice_) {
        Audio::PlaySettings settings; settings.loop=true; settings.volume=volume_;
        voice_=audio_->Play(clip_,settings);
    } else (void)audio_->SetVoiceVolume(voice_,volume_);
}
void FarmRainAudioSystem::Stop() noexcept {
    if (audio_ && voice_) (void)audio_->StopVoice(voice_);
    voice_={}; volume_=0;
}
void FarmRainAudioSystem::Finalize() noexcept {
    Stop();
    if (audio_ && clip_) (void)audio_->UnloadAudio(clip_);
    clip_={}; audio_=nullptr;
}
