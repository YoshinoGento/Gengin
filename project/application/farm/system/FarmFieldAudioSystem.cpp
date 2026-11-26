#include "farm/system/FarmFieldAudioSystem.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace farm {
namespace {
struct Effect { FieldCue cue; const char* file; float gain, interval; bool water; };
constexpr Effect kEffects[]{
    {FieldCue::Hoe,"hoe",.90f,.10f,false}, {FieldCue::Seed,"seed",.69f,.30f,false},
    {FieldCue::Cover,"cover",.50f,.25f,false}, {FieldCue::Pour,"pour",.82f,.36f,true},
    {FieldCue::Pull,"pull",.90f,.10f,false}, {FieldCue::Pick,"pick",.82f,.10f,false},
    {FieldCue::Cut,"cut",.84f,.10f,false}, {FieldCue::Basket,"basket",.75f,.12f,false},
    {FieldCue::HeavyBasket,"heavy_basket",.88f,.18f,false}, {FieldCue::Bend,"bend",.72f,.56f,true},
    {FieldCue::Seep,"seep",.66f,.55f,true}, {FieldCue::Ripple,"ripple",.42f,.44f,true},
    {FieldCue::BrushSoil,"brush_soil",.38f,.38f,false}, {FieldCue::BrushLeaf,"brush_leaf",.46f,.40f,false},
    {FieldCue::Equip,"equip",.45f,.13f,false}
};
static_assert(std::size(kEffects)==static_cast<std::size_t>(FieldCue::Count));
}
bool FarmFieldAudioSystem::Initialize(Audio* audio) {
    Finalize();
    if(!audio) return false;
    audio_=audio;
    bool complete=true;
    for(const auto& e:kEffects) {
        auto& s=slots_[static_cast<size_t>(e.cue)];
        s.clip=audio_->LoadAudio(std::string("Resources/farm/field/audio/")+e.file+".wav");
        if(!s.clip) complete=false;
    }
    return complete;
}
FieldCues FarmFieldAudioSystem::Update(FieldCues cues,bool audible,float dt,int effects,int water,FarmTool tool,int seed) {
    FieldCues played;
    if(!audio_ || !audible || !std::isfinite(dt)) { Stop(); return played; }
    if(tracking_ && (tool!=tool_ || (tool==FarmTool::Seed && seed!=seed_))) Mark(cues,FieldCue::Equip);
    tracking_=true; tool_=tool; seed_=seed;
    for(const auto& e:kEffects) {
        const auto i=static_cast<size_t>(e.cue);
        auto& s=slots_[i];
        s.wait=(std::max)(0.f,s.wait-std::clamp(dt,0.f,.1f));
        const int level=std::clamp(e.water?water:effects,0,100);
        if(!level && s.voice) { (void)audio_->StopVoice(s.voice); s.voice={}; }
        if(!level || !s.clip || !cues[i] || s.wait>0) continue;
        if(s.voice) (void)audio_->StopVoice(s.voice);
        Audio::PlaySettings settings;
        settings.volume=level*.01f*e.gain;
        s.voice=audio_->Play(s.clip,settings);
        if(s.voice) { played.set(i); s.wait=e.interval; }
    }
    return played;
}
void FarmFieldAudioSystem::Stop() noexcept {
    for(auto& s:slots_) {
        if(audio_ && s.voice) (void)audio_->StopVoice(s.voice);
        s.voice={}; s.wait=0;
    }
    tracking_=false;
}
void FarmFieldAudioSystem::Finalize() noexcept {
    Stop();
    for(auto& s:slots_) {
        if(audio_ && s.clip) (void)audio_->UnloadAudio(s.clip);
        s.clip={};
    }
    audio_=nullptr;
}
}
