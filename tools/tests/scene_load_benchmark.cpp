#include "Game.h"
#include "scene/SceneManager.h"
#include "base/Logger.h"
#include "base/DirectXCommon.h"
#include "base/LoadTiming.h"
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// Separate entry point: executes production initialization/draw without injecting OS input.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    using Clock = std::chrono::steady_clock;
    if (!std::filesystem::is_regular_file("scene_load_profile.enabled") ||
        std::filesystem::current_path().generic_string().find("generated/codex_checks/") == std::string::npos) return 2;
    std::ofstream result("scene_load_benchmark_result.txt");
    if (!result) return 2;
    Game game;
    loading::enabled = true;
    bool initialized = false;
    try {
        game.Initialize();
        initialized = true;
        auto* scenes = SceneManager::GetInstance();
        const auto started = Clock::now();
        unsigned requested = 0, completed = 0, settledFrames = 0;
        bool sawLoading = false;
        while (!game.IsEndRequest() && completed < 4) {
            if (Clock::now()-started > std::chrono::seconds(120)) throw std::runtime_error("benchmark timeout");
            game.Update();
            game.Draw();
            if (requested > completed) {
                sawLoading |= scenes->GetCurrentScene() == nullptr;
                if (sawLoading && scenes->GetCurrentScene()) {
                    ++completed;
                    settledFrames = 0;
                    result << "completed=" << completed << '\n';
                    const auto& shaders = game.GetDxCommon()->GetShaderCompileStats();
                    result << "shaders: requests=" << shaders.requests << ", compiles=" << shaders.compiles
                        << ", hits=" << shaders.hits << ", bytes=" << shaders.retainedBytes
                        << ", compileMs=" << shaders.compileMs << '\n';
                    result.flush();
                    for (std::size_t i = 0; i < loading::counters.size(); ++i) {
                        const auto& c = loading::counters[i];
                        result << "asset: " << loading::kStageNames[i] << ", calls=" << c.calls
                            << ", ms=" << c.totalMs << ", maxMs=" << c.maximumMs << '\n';
                    }
                    result.flush();
                }
            } else if (++settledFrames > 45) {
                loading::Reset();
                if (scenes->ChangeSceneWithRipple(requested%2 == 0 ? "GAMEPLAY" : "TITLE", {.64f,.82f})) {
                    ++requested;
                    sawLoading = false;
                }
            }
        }
        // Finish the final reveal and its GPU submission before normal teardown.
        for (int i=0;i<60 && !game.IsEndRequest();++i) { game.Update(); game.Draw(); }
        initialized = false;
        game.Finalize();
        result << (completed == 4 ? "PASS" : "INTERRUPTED") << '\n';
        return completed == 4 ? 0 : 3;
    } catch (const std::exception& error) {
        result << "FAILED: " << error.what() << '\n';
        if (initialized) {
            try { game.Finalize(); }
            catch (...) { result << "FAILED: cleanup\n"; }
        }
        return 1;
    }
}
