#include "farm/system/FarmRainAudioSystem.h"
#include <filesystem>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

void Check(bool value, std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("rain audio line " + std::to_string(at.line()));
}
struct ComSession {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComSession() { if(SUCCEEDED(result)) CoUninitialize(); }
};
int main() {
    ComSession com; Check(SUCCEEDED(com.result));
    Audio backend; Check(backend.Initialize());
    const auto other=backend.LoadAudio("project/Resources/transition/audio/ledger.wav"); Check(static_cast<bool>(other));
    Audio::PlaySettings settings; settings.loop=true; settings.startPaused=true; settings.volume=0;
    const auto otherVoice=backend.Play(other,settings); Check(static_cast<bool>(otherVoice));
    const auto base=backend.GetStatistics();
    Check(FarmRainAudioSystem::VolumeFor(1,-1)==0 && FarmRainAudioSystem::VolumeFor(1,0)==0);
    Check(FarmRainAudioSystem::VolumeFor(2,101)==FarmRainAudioSystem::VolumeFor(1,100));
    Check(FarmRainAudioSystem::VolumeFor(std::numeric_limits<float>::quiet_NaN(),100)==0);
    Check(std::abs(FarmRainAudioSystem::VolumeFor(1,40)-.096f)<.00001f);
    {
        FarmRainAudioSystem sound;
        Check(!sound.Initialize(nullptr)); sound.Update(1,40,true,.1f); sound.Stop(); sound.Finalize();
        Check(!sound.Initialize(&backend));
        Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
        std::filesystem::current_path("project");
        for(int run=0;run<3;++run) {
            Check(sound.Initialize(&backend));
            Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount+1);
            sound.Update(1,0,true,.1f); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.Update(1,40,true,-1); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            for(int i=0;i<100;++i) sound.Update(1,40,true,.016f);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount+1);
            sound.Stop(); sound.Stop();
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount && backend.IsVoiceActive(otherVoice));
            sound.Update(1,40,true,.1f);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount+1);
            sound.Update(1,0,true,.1f); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.Update(1,40,true,.1f); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount+1);
            sound.Update(1,40,false,.1f); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.Update(1,40,true,.1f);
            for(int i=0;i<100;++i) sound.Update(0,40,true,.016f);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.Update(1,40,true,.1f); sound.Finalize(); sound.Finalize();
            Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
            Check(backend.GetStatistics().decodedPcmBytes==base.decodedPcmBytes);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount && backend.IsVoiceActive(otherVoice));
        }
        Check(sound.Initialize(&backend)); sound.Update(1,40,true,.1f);
    }
    Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
    Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
    Check(backend.StopVoice(otherVoice) && backend.UnloadAudio(other)); backend.Finalize();
    std::cout << "PASS: rain XAudio2 null/missing, volume/mute, single voice, focus, fade-out, reentry and destruction\n";
}
