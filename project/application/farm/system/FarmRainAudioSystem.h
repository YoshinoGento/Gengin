#pragma once
#include "audio/Audio.h"
#include <algorithm>
#include <cmath>

// Private looping clip; at most one voice. Framework Audio must outlive this owner.
class FarmRainAudioSystem final {
public:
    FarmRainAudioSystem()=default;
    ~FarmRainAudioSystem() { Finalize(); }
    FarmRainAudioSystem(const FarmRainAudioSystem&)=delete;
    FarmRainAudioSystem& operator=(const FarmRainAudioSystem&)=delete;
    bool Initialize(Audio* audio);
    void Update(float rain, int environmentPercent, bool audible, float realSeconds);
    void Stop() noexcept;
    void Finalize() noexcept;
    static float VolumeFor(float rain, int percent) noexcept {
        return std::isfinite(rain) ? std::clamp(rain,0.0f,1.0f)*std::clamp(percent,0,100)*.01f*.24f : 0;
    }
private:
    Audio* audio_=nullptr;
    AudioClipHandle clip_{};
    AudioVoiceHandle voice_{};
    float volume_=0;
};
