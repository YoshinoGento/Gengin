#include "farm/system/FarmPestAudioSystem.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {
void Check(bool value, std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("pest audio line " + std::to_string(at.line()));
}
struct ComSession {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComSession() { if(SUCCEEDED(result)) CoUninitialize(); }
};
}
int main() {
    ComSession com; Check(SUCCEEDED(com.result));
    Audio backend; Check(backend.Initialize());
    const auto other=backend.LoadAudio("project/Resources/transition/audio/ledger.wav"); Check(static_cast<bool>(other));
    Audio::PlaySettings settings; settings.loop=true; settings.startPaused=true; settings.volume=0;
    const auto otherVoice=backend.Play(other,settings); Check(static_cast<bool>(otherVoice));
    const auto base=backend.GetStatistics();
    Check(FarmPestAudioSystem::VolumeFor(-1)==0 && FarmPestAudioSystem::VolumeFor(0)==0);
    Check(FarmPestAudioSystem::VolumeFor(101)==FarmPestAudioSystem::VolumeFor(100));
    Check(std::abs(FarmPestAudioSystem::VolumeFor(40)-.072f)<.00001f);
    {
        FarmPestAudioSystem sound;
        Check(!sound.Initialize(nullptr)); sound.PlayCured(40); sound.Stop(); sound.Finalize();
        Check(!sound.Initialize(&backend)); // Root cwd deliberately lacks Resources/farm/pests.
        Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
        std::filesystem::current_path("project");
        for(int run=0;run<3;++run) {
            Check(sound.Initialize(&backend));
            Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount+1);
            sound.PlayCured(0); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            for(int i=0;i<20;++i) sound.PlayCured(40);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount+1);
            sound.Stop(); Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.PlayCured(40);
            for(int i=0;i<50;++i) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f); }
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
            sound.PlayCured(40); sound.Finalize(); sound.Finalize();
            Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
            Check(backend.GetStatistics().decodedPcmBytes==base.decodedPcmBytes);
            Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount && backend.IsVoiceActive(otherVoice));
        }
        Check(sound.Initialize(&backend)); sound.PlayCured(40);
    }
    Check(backend.GetStatistics().loadedClipCount==base.loadedClipCount);
    Check(backend.GetStatistics().activeVoiceCount==base.activeVoiceCount);
    Check(backend.StopVoice(otherVoice) && backend.UnloadAudio(other)); backend.Finalize();
    std::cout << "PASS: pest XAudio2 null/missing clip, volume, single voice, stop, completion, reentry, lifetime\n";
}
