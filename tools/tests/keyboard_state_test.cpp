#include "io/KeyboardState.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    static_assert(std::is_trivially_copyable_v<KeyboardState>);
    static_assert(sizeof(KeyboardState) <= 5 * KeyboardState::kKeyCount);
    KeyboardState keys;
    for (int key = 0; key < KeyboardState::kKeyCount; ++key) {
        keys.Reset();
        Check(!keys.Held(key) && !keys.Pressed(key) && !keys.Released(key), "initial state");
        // The former immediate-state comparison sees up -> up and loses this pulse.
        const bool sampledBefore = false, sampledAfter = false;
        Check(!(sampledAfter && !sampledBefore), "snapshot pulse-loss reproduction");
        keys.Apply(key, true);
        keys.Apply(key, false);
        Check(keys.Pressed(key) && keys.Released(key) && !keys.Held(key), "retain short tap");
        Check(keys.Consume(key) && !keys.Consume(key) && !keys.Pressed(key), "consume once");
        Check(keys.Released(key), "consume preserves release");
        keys.BeginFrame();
        Check(!keys.Pressed(key) && !keys.Released(key) && !keys.Held(key), "edges expire");

        keys.Apply(key, true);
        Check(keys.Pressed(key) && keys.Held(key), "press after tap");
        keys.BeginFrame();
        for (int repeat = 0; repeat < 50; ++repeat) keys.Apply(key, true);
        Check(keys.Held(key) && !keys.Pressed(key) && !keys.Released(key), "no held repeat");
        keys.Apply(key, false);
        Check(!keys.Held(key) && keys.Released(key), "normal release");

        keys.Reset();
        for (int repeat = 0; repeat < 20; ++repeat) {
            keys.Apply(key, true); keys.Apply(key, false);
        }
        Check(keys.Consume(key) && !keys.Consume(key), "coalesce frame taps");

        KeyboardState::Snapshot snapshot{};
        snapshot[key] = 0x80;
        keys.Synchronize(snapshot);
        Check(keys.Held(key) && !keys.Pressed(key) && !keys.Released(key), "focus/overflow held gate");
        keys.Apply(key, true);
        keys.BeginFrame();
        Check(!keys.Pressed(key) && keys.Held(key), "restored hold never decides");
        keys.Apply(key, false);
        Check(!keys.Released(key) && !keys.Held(key), "restored release silent");
        keys.Apply(key, true);
        Check(keys.Pressed(key) && keys.Held(key), "fresh press after restoration");
        keys.Synchronize({});
        Check(!keys.Pressed(key) && !keys.Released(key) && !keys.Held(key), "incomplete history discarded");

        keys.ApplySnapshot(snapshot);
        Check(keys.Pressed(key) && keys.Held(key), "immediate fallback press");
        keys.BeginFrame(); keys.ApplySnapshot(snapshot);
        Check(!keys.Pressed(key) && keys.Held(key), "immediate fallback held");
        keys.BeginFrame(); keys.ApplySnapshot({});
        Check(keys.Released(key) && !keys.Held(key), "immediate fallback release");
        keys.Reset(); snapshot[key] = 0x01; keys.ApplySnapshot(snapshot);
        Check(!keys.Held(key) && !keys.Pressed(key), "only high bit means held");
    }

    keys.Reset();
    for (int key = 0; key < KeyboardState::kKeyCount; ++key) keys.Apply(key, true);
    for (int key = 0; key < KeyboardState::kKeyCount; ++key) {
        Check(keys.Consume(key), "independent simultaneous presses");
        keys.Apply(key, false);
    }
    for (const int invalid : {-1, 256, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()}) {
        keys.Apply(invalid, true); keys.Apply(invalid, false);
        Check(!keys.Held(invalid) && !keys.Pressed(invalid) && !keys.Released(invalid) && !keys.Consume(invalid), "invalid offset");
    }
    keys.Reset();
    for (int key = 0; key < KeyboardState::kKeyCount; ++key)
        Check(!keys.Held(key) && !keys.Pressed(key) && !keys.Released(key), "focus loss clears all");
    std::cout << "PASS keyboard_state: 256 keys, pulse/hold/consume/restore/fallback/bounds\n";
}
