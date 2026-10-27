#pragma once
#include "title/TitleAudioSettingsSystem.h"
#include "2d/Sprite.h"
#include <array>
class Input;
class WinApp;
namespace title {
class TitleAudioSettingsView final {
public:
    bool Initialize(SpriteCommon* common);
    [[nodiscard]] AudioSettingsInput ReadInput(const Input& input, const WinApp& window) const;
    void Draw(const AudioSettingsFrame& frame);
private:
    void Box(std::size_t index, Vector2 position, Vector2 size, Vector4 color);
    void Label(std::size_t index, int row, Vector2 position, float width);
    void Percent(std::size_t index, int value, Vector2 position);
    SpriteCommon* common_ = nullptr;
    std::array<Sprite,5+kAudioChannelCount*3> shapes_;
    std::array<Sprite,5+kAudioChannelCount> labels_;
    std::array<Sprite,kAudioChannelCount*4> digits_;
};
}
