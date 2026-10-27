#include "title/TitleAudioSettingsView.h"
#include "title/TitlePresentationLayout.h"
#include "io/Input.h"
#include "base/WinApp.h"
namespace title {
bool TitleAudioSettingsView::Initialize(SpriteCommon* common) {
    common_ = nullptr;
    if (!common) return false;
    for (auto& sprite : shapes_) if (!sprite.Initialize(common,"Resources/farm/white.png")) return false;
    for (auto& sprite : labels_) if (!sprite.Initialize(common,"Resources/title/audio_settings.png")) return false;
    for (auto& sprite : digits_) if (!sprite.Initialize(common,"Resources/title/audio_settings.png")) return false;
    common_ = common;
    return true;
}
AudioSettingsInput TitleAudioSettingsView::ReadInput(const Input& input, const WinApp& window) const {
    AudioSettingsInput result;
    result.toggle = input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::Start);
    result.back = input.TriggerGamepadButton(InputGamepadButton::B);
    result.confirm = input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) || input.TriggerGamepadButton(InputGamepadButton::A);
    result.up = input.TriggerKey(InputKey::ArrowUp) || input.TriggerGamepadButton(InputGamepadButton::DPadUp);
    result.down = input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::Tab) || input.TriggerGamepadButton(InputGamepadButton::DPadDown);
    result.left = input.TriggerKey(InputKey::ArrowLeft) || input.TriggerGamepadButton(InputGamepadButton::DPadLeft);
    result.right = input.TriggerKey(InputKey::ArrowRight) || input.TriggerGamepadButton(InputGamepadButton::DPadRight);
#ifndef USE_IMGUI
    const auto width = window.GetClientWidth(), height = window.GetClientHeight();
    if (width && height) {
        const auto mouse = input.GetMousePosition();
        result.pointer = {mouse.x*kWidth/static_cast<float>(width),mouse.y*kHeight/static_cast<float>(height)};
        result.pointerValid = true;
        result.pressed = input.TriggerMouseButton(InputMouseButton::Left);
        result.held = input.PushMouseButton(InputMouseButton::Left);
    }
#else
    (void)window;
#endif
    return result;
}
void TitleAudioSettingsView::Box(std::size_t index, Vector2 position, Vector2 size, Vector4 color) {
    auto& sprite = shapes_[index];
    sprite.SetPosition(position); sprite.SetSize(size); sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void TitleAudioSettingsView::Label(std::size_t index, int row, Vector2 position, float width) {
    auto& sprite = labels_[index];
    sprite.SetTextureRect({0,row*40.0f},{width,40}); sprite.SetSize({width,40});
    sprite.SetPosition(position); sprite.Update(); sprite.Draw();
}
void TitleAudioSettingsView::Percent(std::size_t index, int value, Vector2 position) {
    const std::array<int,4> glyphs{value/100,(value/10)%10,value%10,10};
    for (std::size_t i=0; i<glyphs.size(); ++i) {
        if ((i==0 && value<100) || (i==1 && value<10)) continue;
        auto& sprite = digits_[index+i];
        sprite.SetTextureRect({glyphs[i]*24.0f,280},{24,32}); sprite.SetSize({24,32});
        sprite.SetPosition({position.x+i*24.0f,position.y}); sprite.Update(); sprite.Draw();
    }
}
void TitleAudioSettingsView::Draw(const AudioSettingsFrame& frame) {
    if (!common_) return;
    common_->PreDraw();
    constexpr Vector4 panel{.075f,.12f,.105f,.98f}, control{.2f,.31f,.29f,1}, accent{.64f,.85f,.8f,1};
    Box(0,kAudioSettingsButton.position,kAudioSettingsButton.size,control); Label(0,4,{1088,48},140);
    if (!frame.open) return;
    Box(1,{0,0},{kWidth,kHeight},{0,0,0,.55f});
    Box(2,kAudioSettingsPanel.position,kAudioSettingsPanel.size,panel); Label(1,0,{356,230},280);
    constexpr std::array<int,kAudioChannelCount> labelRows{1,2,8};
    for (std::size_t i=0; i<kAudioChannelCount; ++i) {
        const float y = kAudioSliderY[i], width = kAudioSliderWidth*frame.volume[i]*.01f;
        Label(2+i,labelRows[i],{356,y-20},176);
        Box(3+i*3,{kAudioSliderX,y-4},{kAudioSliderWidth,8},control);
        if (width>0) Box(4+i*3,{kAudioSliderX,y-4},{width,8},accent);
        Box(5+i*3,{kAudioSliderX+width-8,y-16},{16,32},frame.selected==static_cast<int>(i) ? Vector4{1,.9f,.48f,1}:accent);
        Percent(i*4,frame.volume[i],{800,y+26});
    }
    constexpr auto buttons = 3+kAudioChannelCount*3, labels = 2+kAudioChannelCount;
    Box(buttons,kAudioSettingsReset.position,kAudioSettingsReset.size,control); Label(labels,6,{376,576},140);
    Box(buttons+1,kAudioSettingsBack.position,kAudioSettingsBack.size,control); Label(labels+1,3,{816,576},92);
    if (frame.saveFailed) Label(labels+2,5,{660,230},280);
}
}
