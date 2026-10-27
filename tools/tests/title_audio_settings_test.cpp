#include "title/TitleAudioSettingsSystem.h"
#include "io/JsonFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
namespace {
void Check(bool result, std::source_location at = std::source_location::current()) {
    if (!result) throw std::runtime_error("settings assertion at line "+std::to_string(at.line()));
}
title::AudioSettingsInput Click(float x, float y, bool held = true) {
    title::AudioSettingsInput input;
    input.pointer = {x,y}; input.pointerValid = true; input.pressed = true; input.held = held;
    return input;
}
}
int main() {
    const std::filesystem::path dir = "generated/codex_checks/title_audio_settings_tests";
    std::filesystem::create_directories(dir);
    const auto path = (dir/"settings.json").string();
    std::filesystem::remove(path);
    title::TitleAudioSettingsSystem system;
    system.Initialize(path);
    Check(system.GetFrame().volume == std::array<int,3>{40,40,40});
    Check(system.GetContestVolume()==40);
    for (std::size_t i=0;i<title::kAudioChannelCount;++i) {
        const auto& hit=title::kAudioSliderHits[i];
        Check(title::kAudioSettingsPanel.Contains(hit.position));
        Check(title::kAudioSettingsPanel.Contains({hit.position.x+hit.size.x,hit.position.y+hit.size.y}));
        Check(hit.position.y+hit.size.y<title::kAudioSettingsReset.position.y);
        if(i>0) Check(title::kAudioSliderHits[i-1].position.y+title::kAudioSliderHits[i-1].size.y<hit.position.y);
    }
    const auto mix = system.Apply(title::MakeAudioMix(0,0));
    Check(std::abs(mix.ambience[0]-.18f)<.00001f && std::abs(mix.water-.12f)<.00001f);
    for (int i=0; i<4000; ++i) {
        const auto scaled = system.Apply(title::MakeAudioMix(i*.01f,0));
        float total = 0; for (auto gain : scaled.ambience) total += gain;
        Check(total<=title::kAmbienceGain*.4f+.00001f && scaled.water<=title::kWaterGain*.4f+.00001f);
    }
    Check(system.Update(Click(640,650)));
    Check(system.GetWaterClick() && system.GetWaterClick()->x == 640 && system.GetWaterClick()->y == 650);
    Check(system.Update(Click(500,610)) && system.Update(Click(780,690)));
    auto holdStart = Click(640,650); holdStart.pressed = false;
    Check(!system.Update(holdStart));
    Check(!system.GetWaterClick());
    Check(system.Update(Click(640,500)));
    Check(system.GetWaterClick() && system.GetWaterClick()->y == 500);
    Check(!system.Update(Click(-1,500)) && !system.GetWaterClick());
    Check(system.Update(Click(0,0)) && system.Update(Click(1280,720)));
    Check(!system.Update(Click(1281,720)) && !system.GetWaterClick());
    Check(!system.Update(Click(std::numeric_limits<float>::quiet_NaN(),500)) && !system.GetWaterClick());
    Check(!system.Update(Click(1100,60)) && system.GetFrame().open);
    Check(!system.GetWaterClick());
    Check(!system.Update(Click(640,650))); // Start rectangle is modal-blocked.
    Check(!system.GetWaterClick());
    Check(!system.Update(Click(title::kAudioSliderX,310)) && system.GetFrame().volume[0]==0);
    auto drag = Click(2000,310); drag.pressed = false;
    Check(!system.Update(drag) && system.GetFrame().volume[0]==100);
    Check(!JsonFile::Exists(path)); // Drag frames do not write settings.
    system.Update({});
    drag.pointer.x = 600; system.Update(drag);
    Check(system.GetFrame().volume[0]==100); // Release ends capture.
    auto nan = Click(std::numeric_limits<float>::quiet_NaN(),310); system.Update(nan);
    Check(system.GetFrame().volume[0]==100);
    system.Update(Click(title::kAudioSliderX+title::kAudioSliderWidth*.25f,398,false));
    Check(system.GetFrame().volume[1]==25);
    title::AudioSettingsInput right; right.right = true;
    system.Update(right); Check(system.GetFrame().volume[1]==30);
    title::AudioSettingsInput up; up.up = true;
    system.Update(up); Check(system.GetFrame().selected==0);
    system.Update(up); Check(system.GetFrame().selected==2);
    system.Update(right); Check(system.GetContestVolume()==45);
    system.Update(title::AudioSettingsInput{.down=true}); Check(system.GetFrame().selected==0);
    system.Update(title::AudioSettingsInput{.up=true,.down=true}); Check(system.GetFrame().selected==0);
    system.Update(Click(816,588));
    Check(!system.GetFrame().open && JsonFile::Exists(path));
    title::TitleAudioSettingsSystem restored; restored.Initialize(path);
    Check(restored.GetFrame().volume == std::array<int,3>{100,30,45});
    Check(!restored.Update(Click(1100,60)));
    Check(!restored.Update(title::AudioSettingsInput{.confirm=true}) && !restored.GetFrame().open);
    Check(restored.Update(title::AudioSettingsInput{.confirm=true}));
    Check(!restored.GetWaterClick()); // Keyboard starts use the transition system's default origin.
    Check(!restored.Update(title::AudioSettingsInput{.toggle=true,.confirm=true}));
    restored.Update(Click(400,588));
    Check(restored.GetFrame().volume==std::array<int,3>{40,40,40});
    Check(restored.Save());
    restored.Update(Click(title::kAudioSliderX,486));
    Check(restored.GetContestVolume()==0);
    const auto stillAudible=restored.Apply(title::MakeAudioMix(0,0));
    Check(stillAudible.ambience==mix.ambience && stillAudible.water==mix.water);
    auto contestDrag=Click(2000,486); contestDrag.pressed=false;
    restored.Update(contestDrag); Check(restored.GetContestVolume()==100);
    restored.Update({});
    restored.Update(Click(title::kAudioSliderX+title::kAudioSliderWidth*.35f,486));
    Check(restored.GetContestVolume()==35);
    restored.Update(Click(title::kAudioSliderX,310));
    restored.Update(Click(title::kAudioSliderX,398));
    const auto muted = restored.Apply(title::MakeAudioMix(9,0));
    for (auto gain : muted.ambience) Check(gain==0);
    Check(muted.water==0);
    Check(restored.GetContestVolume()==35); // Background muting does not mute judgments.
    Check(restored.Save());
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,3>{0,0,35});
    Check(JsonFile::Save(path,{{"version",1},{"environmentPercent",-300},{"waterPercent",1e100}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,3>{0,100,40});
    Check(JsonFile::Save(path,{{"version",1},{"environmentPercent","loud"},{"waterPercent",nullptr}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,3>{40,40,40});
    for (const auto& value : {nlohmann::json(-5),nlohmann::json(999),nlohmann::json("loud"),nlohmann::json(nullptr)}) {
        Check(JsonFile::Save(path,{{"version",1},{"contestPercent",value}}));
        restored.Initialize(path);
        Check(restored.GetContestVolume()==(value.is_number() ? (value.get<int>()<0 ? 0 : 100) : 40));
    }
    Check(JsonFile::Save(path,{{"version",2},{"environmentPercent",100}}));
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,3>{40,40,40});
    { std::ofstream file(path); file << "broken JSON"; }
    restored.Initialize(path); Check(restored.GetFrame().volume==std::array<int,3>{40,40,40});
    const auto blocker = dir/"not_a_directory";
    { std::ofstream file(blocker); file << "fixture"; }
    restored.Initialize((blocker/"settings.json").string());
    restored.Update(title::AudioSettingsInput{.toggle=true}); restored.Update(right);
    Check(!restored.Save() && restored.GetFrame().saveFailed);
    Check(restored.GetFrame().volume[0]==45);
    std::cout << "PASS title volume: defaults/mix/modal/drag/release/keyboard/persistence/corrupt/failure\n";
}
