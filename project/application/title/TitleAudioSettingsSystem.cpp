#include "title/TitleAudioSettingsSystem.h"
#include "title/TitlePresentationLayout.h"
#include "io/JsonFile.h"
#include "base/Logger.h"
#include <Windows.h>
#include <filesystem>

namespace title {
namespace {
int ReadVolume(const nlohmann::json& json, const char* key) {
    const auto found = json.find(key);
    if (found == json.end() || !found->is_number()) return kDefaultTitleVolume;
    const double value = found->get<double>();
    return std::isfinite(value) ? static_cast<int>(std::round(std::clamp(value,0.0,100.0))) : kDefaultTitleVolume;
}
}
void TitleAudioSettingsSystem::Initialize(const std::string& path) {
    frame_ = {}; dragging_ = -1; dirty_ = false; path_ = path;
    waterClick_.reset();
    nlohmann::json json;
    if (!JsonFile::Exists(path_) || !JsonFile::Load(path_,json) || !json.is_object() ||
        !json.contains("version") || json["version"] != 1) return;
    frame_.volume = {ReadVolume(json,"environmentPercent"),ReadVolume(json,"waterPercent"),
        ReadVolume(json,"contestPercent")};
}
void TitleAudioSettingsSystem::SetVolume(int index, int value) noexcept {
    if (index<0 || index>=static_cast<int>(frame_.volume.size())) return;
    auto& current = frame_.volume[static_cast<std::size_t>(index)];
    value = std::clamp(value,0,100);
    if (current != value) { current = value; dirty_ = true; frame_.saveFailed = false; }
}
void TitleAudioSettingsSystem::Close() { Save(); frame_.open = false; dragging_ = -1; }
bool TitleAudioSettingsSystem::Update(const AudioSettingsInput& input) {
    waterClick_.reset();
    if (input.toggle || (!frame_.open && input.pointerValid && input.pressed &&
        kAudioSettingsButton.Contains(input.pointer))) {
        if (frame_.open) Close(); else { frame_.open = true; frame_.selected = 0; }
        return false;
    }
    if (!frame_.open) {
        if (input.pointerValid && input.pressed && std::isfinite(input.pointer.x) && std::isfinite(input.pointer.y) &&
            input.pointer.x >= 0 && input.pointer.x <= kWidth && input.pointer.y >= 0 && input.pointer.y <= kHeight)
            waterClick_ = input.pointer;
        return input.confirm || waterClick_.has_value();
    }
    if (input.back || input.confirm || (input.pointerValid && input.pressed && kAudioSettingsBack.Contains(input.pointer))) {
        Close(); return false;
    }
    if (input.up != input.down) frame_.selected =
        (frame_.selected + (input.down ? 1 : static_cast<int>(kAudioChannelCount)-1)) % static_cast<int>(kAudioChannelCount);
    if (input.left != input.right) SetVolume(frame_.selected,
        frame_.volume[static_cast<std::size_t>(frame_.selected)] + (input.right ? 5 : -5));
    if (input.pointerValid && input.pressed && kAudioSettingsReset.Contains(input.pointer)) {
        for (int i=0; i<static_cast<int>(kAudioChannelCount); ++i) SetVolume(i,kDefaultTitleVolume);
    }
    if (input.pointerValid && input.pressed) {
        for (int i=0; i<static_cast<int>(kAudioChannelCount); ++i) if (kAudioSliderHits[static_cast<std::size_t>(i)].Contains(input.pointer)) {
            dragging_ = i; frame_.selected = i;
        }
    }
    if (!input.held && !input.pressed) dragging_ = -1;
    if (dragging_ >= 0 && input.pointerValid && std::isfinite(input.pointer.x)) {
        const float fraction = std::clamp((input.pointer.x-kAudioSliderX)/kAudioSliderWidth,0.0f,1.0f);
        SetVolume(dragging_,static_cast<int>(std::round(fraction*100)));
    }
    return false;
}
AudioMix TitleAudioSettingsSystem::Apply(const AudioMix& mix) const noexcept {
    auto result = SanitizeAudioMix(mix);
    for (auto& gain : result.ambience) gain *= frame_.volume[0]*.01f;
    result.water *= frame_.volume[1]*.01f;
    return result;
}
bool TitleAudioSettingsSystem::Save() {
    if (!dirty_) return true;
    const std::filesystem::path path(path_);
    std::error_code error;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),error);
    const auto temporary = path.string()+".tmp";
    const nlohmann::json json{{"version",1},{"environmentPercent",frame_.volume[0]},
        {"waterPercent",frame_.volume[1]},{"contestPercent",GetContestVolume()}};
    // Replace only after a complete JSON write; the prior settings survive failed writes.
    const bool success = !error && JsonFile::Save(temporary,json) &&
        MoveFileExW(std::filesystem::path(temporary).c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    frame_.saveFailed = !success;
    if (success) dirty_ = false;
    else Logger::Warning("Title volume settings could not be saved; current session still uses selected levels.");
    return success;
}
}
