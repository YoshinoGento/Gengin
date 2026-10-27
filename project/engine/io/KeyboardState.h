#pragma once

#include <array>
#include <cstdint>

// Buffered edges live for one Input::Update; held state survives between frames.
class KeyboardState final {
public:
    static constexpr int kKeyCount = 256;
    using Snapshot = std::array<std::uint8_t, kKeyCount>;

    void BeginFrame() {
        pressed_ = {};
        released_ = {};
        consumed_ = {};
    }

    void Reset() {
        held_ = {};
        suppressed_ = {};
        BeginFrame();
    }

    void Synchronize(const Snapshot& snapshot) {
        Reset();
        for (int i = 0; i < kKeyCount; ++i) {
            held_[i] = (snapshot[i] & 0x80) != 0;
            suppressed_[i] = held_[i];
        }
    }

    void Apply(int key, bool down) {
        if (!Valid(key)) return;
        const bool changed = held_[key] != down;
        held_[key] = down;
        // Keep held state for release gates, but never decide on focus restoration.
        if (suppressed_[key]) {
            if (!down) suppressed_[key] = false;
            return;
        }
        if (!changed) return;
        if (down) pressed_[key] = true;
        else released_[key] = true;
    }

    void ApplySnapshot(const Snapshot& snapshot) {
        for (int i = 0; i < kKeyCount; ++i) Apply(i, (snapshot[i] & 0x80) != 0);
    }

    bool Held(int key) const { return Valid(key) && held_[key]; }
    bool Pressed(int key) const { return Valid(key) && pressed_[key] && !consumed_[key]; }
    bool Released(int key) const { return Valid(key) && released_[key]; }
    bool Consume(int key) {
        if (!Pressed(key)) return false;
        consumed_[key] = true;
        return true;
    }

private:
    static bool Valid(int key) { return key >= 0 && key < kKeyCount; }
    std::array<bool, kKeyCount> held_{};
    std::array<bool, kKeyCount> pressed_{};
    std::array<bool, kKeyCount> released_{};
    std::array<bool, kKeyCount> consumed_{};
    std::array<bool, kKeyCount> suppressed_{};
};
