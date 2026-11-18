#include "farm/system/FarmContestAudioSystem.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {
void Check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("contest audio assertion at line " + std::to_string(at.line()));
}
struct ComSession {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComSession() { if (SUCCEEDED(result)) CoUninitialize(); }
};
}
int main() {
    ComSession com; Check(SUCCEEDED(com.result));
    Audio backend; Check(backend.Initialize());
    const auto other=backend.LoadAudio("project/Resources/transition/audio/ledger.wav");
    Check(static_cast<bool>(other));
    Audio::PlaySettings paused; paused.startPaused=true; paused.loop=true; paused.volume=0;
    const auto otherVoice=backend.Play(other,paused); Check(static_cast<bool>(otherVoice));
    const auto baseline=backend.GetStatistics();
    using Cue=FarmContestPresentationSystem::Cue;
    for (auto cue : {Cue::Assess,Cue::Recorded,Cue::Passed,Cue::Failed,Cue::Clear,Cue::Drumroll,Cue::Silence,Cue::None,static_cast<Cue>(1000)}) {
        Check(FarmContestAudioSystem::VolumeFor(cue,-500)==0);
        Check(FarmContestAudioSystem::VolumeFor(cue,0)==0);
        Check(FarmContestAudioSystem::VolumeFor(cue,500)==FarmContestAudioSystem::VolumeFor(cue,100));
        float previous=0;
        for (int percent=0;percent<=100;++percent) {
            const auto gain=FarmContestAudioSystem::VolumeFor(cue,percent);
            Check(gain>=previous && gain<=.22f); previous=gain;
        }
    }
    Check(std::abs(FarmContestAudioSystem::VolumeFor(Cue::Assess,40)-.056f)<.00001f);
    Check(std::abs(FarmContestAudioSystem::VolumeFor(Cue::Failed,40)-.088f)<.00001f);
    Check(FarmContestAudioSystem::VolumeFor(Cue::Passed,40)==FarmContestAudioSystem::VolumeFor(Cue::Failed,40));
    Check(FarmContestAudioSystem::VolumeFor(Cue::None,100)==0);
    {
        FarmContestAudioSystem sound;
        Check(!sound.Initialize(nullptr));
        sound.Begin(40); sound.Update(Cue::Failed,true,true); sound.Finalize();
        const auto partial=std::filesystem::path("generated/codex_checks/contest_audio_partial");
        std::filesystem::create_directories(partial);
        std::filesystem::copy_file("project/Resources/farm/contest/audio/assess.wav",partial/"assess.wav",
            std::filesystem::copy_options::overwrite_existing);
        Check(!sound.Initialize(&backend,partial.string()));
        Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount);
        Check(backend.GetStatistics().decodedPcmBytes==baseline.decodedPcmBytes);
        for(int run=0;run<3;++run) {
            Check(sound.Initialize(&backend,"project/Resources/farm/contest/audio"));
            Check(sound.CheersReady());
            Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount+5+farm::cheerassets::kClips.size());
            Check(backend.GetStatistics().decodedPcmBytes==baseline.decodedPcmBytes+(72960+220800+48000+farm::cheerassets::kPcmBytes)*2);
            sound.Begin(0); sound.Update(Cue::Passed,true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            sound.Begin(-500); sound.Update(Cue::Failed,true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            sound.Begin(40);
            sound.Update(static_cast<Cue>(1000),true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            for(auto cue:{Cue::Assess,Cue::Recorded,Cue::Passed,Cue::Failed,Cue::Clear,Cue::Drumroll}) {
                sound.Update(cue,true,true);
                Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
                sound.Update(Cue::None,true,false);
                Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
                sound.Update(Cue::None,true,true);
                Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
                sound.Update(Cue::Silence,true,true);
                Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            }
            sound.Update(Cue::Drumroll,true,true);
            sound.Update(Cue::None,true,false);
            for(int i=0;i<115;++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f);
            }
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
            sound.Update(Cue::None,true,true);
            for(int i=0;i<115;++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f);
            }
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            sound.Update(Cue::Drumroll,true,true); sound.Update(Cue::Clear,true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
            sound.Update(Cue::None,true,false); sound.Update(Cue::Silence,true,false);
            sound.Update(Cue::None,true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            for(int i=0;i<100;++i) sound.Update(Cue::Assess,true,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
            sound.Update(Cue::Failed,false,true);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            sound.Update(Cue::Passed,true,true);
            for(int i=0;i<85;++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f);
            }
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            sound.Update(Cue::Failed,true,true);
            sound.Finalize(); sound.Finalize();
            Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount);
            Check(backend.GetStatistics().decodedPcmBytes==baseline.decodedPcmBytes);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
            Check(backend.IsVoiceActive(otherVoice));
        }
        Check(sound.Initialize(&backend,"project/Resources/farm/contest/audio"));
        sound.Begin(40); sound.Update(Cue::Failed,true,true);
    }
    Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount);
    Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
    {
        using Cheer=FarmContestCheerSystem;
        using Pair=Cheer::Pair;
        using Side=Cheer::Side;
        FarmContestAudioSystem sound;
        Check(sound.Initialize(&backend,"project/Resources/farm/contest/audio"));
        Cheer::Frame left{Pair::PumpkinPot,Side::Left,1,1,true,1};
        auto right=left; right.side=Side::Right;
        for(int percent:{0,-1,40,100,1000}) {
            sound.Begin(percent);
            sound.Update(Cue::Clear,true,true,left);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+(percent>0?2:0));
            for(int i=0;i<50;++i) sound.Update(Cue::None,true,true,left);
            Check(backend.GetStatistics().activeVoiceCount<=baseline.activeVoiceCount+(percent>0?2:0));
            sound.Update(Cue::None,true,true,right);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+(percent>0?2:0));
            sound.Update(Cue::Silence,true,false,right);
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount && backend.IsVoiceActive(otherVoice));
        }
        sound.Begin(40); sound.Update(Cue::Clear,true,true,left);
        sound.Update(Cue::None,true,false,left);
        const int speechTicks=static_cast<int>(std::ceil((Cheer::VoiceSeconds(Pair::PumpkinPot,Side::Left)+.30f)/.01f));
        for(int i=0;i<speechTicks;++i) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f); }
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+2);
        sound.Update(Cue::None,true,true,left);
        for(int i=0;i<speechTicks;++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); backend.Update(.01f);
            sound.Update(Cue::None,true,true,left);
        }
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1); // Speech ended, not restarted.
        sound.Update(Cue::None,true,true,right);
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+2);
        sound.Update(Cue::None,false,true,right);
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
        for(int pair=0;pair<static_cast<int>(Pair::Count);++pair) for(auto side:{Side::Left,Side::Right}) {
            sound.Begin(40);
            sound.Update(Cue::None,true,true,{static_cast<Pair>(pair),side,1,1,true,1});
            Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
            sound.Stop(); Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
        }
        sound.Begin(40);
        sound.Update(Cue::None,true,true,{Pair::Count,Side::Left,1,1,true,1});
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
        auto silent=left; silent.voiceReady=false;
        sound.Update(Cue::None,true,true,silent);
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount);
        const auto partial=std::filesystem::path("generated/codex_checks/contest_speech_partial");
        std::filesystem::create_directories(partial/"cheers");
        for(const auto file:{"assess.wav","passed.wav","failed.wav","clear.wav","drumroll.wav","cheers/Perfect_L.wav"})
            std::filesystem::copy_file(std::filesystem::path("project/Resources/farm/contest/audio")/file,partial/file,
                std::filesystem::copy_options::overwrite_existing);
        Check(sound.Initialize(&backend,partial.string()) && !sound.CheersReady());
        Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount+5);
        sound.Begin(40); sound.Update(Cue::Clear,true,true,left);
        Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount+1);
    }
    Check(backend.GetStatistics().loadedClipCount==baseline.loadedClipCount);
    Check(backend.GetStatistics().decodedPcmBytes==baseline.decodedPcmBytes);
    Check(backend.GetStatistics().activeVoiceCount==baseline.activeVoiceCount && backend.IsVoiceActive(otherVoice));
    Check(backend.StopVoice(otherVoice)); Check(backend.UnloadAudio(other));
    backend.Finalize();
    std::cout << "PASS: contest XAudio2 two spectator voices,32clips,subtitle trigger,mute,pause/resume,no repeated speech,skip,optional fallback,ownership and existing cues\n";
}
