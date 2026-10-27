#pragma once
#include "title/TitleAudioMix.h"
#include "title/TitleAudioSettingsLayout.h"
#include <string>
#include <optional>

namespace title {
inline constexpr int kDefaultTitleVolume = 40;
struct AudioSettingsInput {
    Vector2 pointer{};
    bool pointerValid = false, pressed = false, held = false;
    bool toggle = false, confirm = false, back = false;
    bool up = false, down = false, left = false, right = false;
};
struct AudioSettingsFrame {
    std::array<int,kAudioChannelCount> volume{kDefaultTitleVolume,kDefaultTitleVolume,kDefaultTitleVolume};
    bool open = false, saveFailed = false;
    int selected = 0;
};
class TitleAudioSettingsSystem final {
public:
    void Initialize(const std::string& path = "Settings/runtime/title_audio.json");
    // Returns a start notification only when the settings panel does not consume input.
    bool Update(const AudioSettingsInput& input);
    bool Save();
    [[nodiscard]] const AudioSettingsFrame& GetFrame() const noexcept { return frame_; }
    [[nodiscard]] int GetContestVolume() const noexcept { return frame_.volume[kContestAudioChannel]; }
    [[nodiscard]] AudioMix Apply(const AudioMix& mix) const noexcept;
    [[nodiscard]] const std::optional<Vector2>& GetWaterClick() const noexcept { return waterClick_; }
private:
    void SetVolume(int index, int value) noexcept;
    void Close();
    AudioSettingsFrame frame_{};
    std::string path_;
    int dragging_ = -1;
    bool dirty_ = false;
    std::optional<Vector2> waterClick_;
};
}
