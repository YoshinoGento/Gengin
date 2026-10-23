#pragma once

namespace editor {
class GamePlayEditorBridge;

// Only scenes with a gameplay editor implement this capability. No scene owns EditorShell.
class GamePlayEditorScene {
public:
    virtual ~GamePlayEditorScene() = default;
    virtual GamePlayEditorBridge& GetEditorBridge() noexcept = 0;
    [[nodiscard]] virtual bool IsFarmGameMode() const noexcept = 0;
    virtual bool SetFarmGameMode(bool enabled) = 0;
    virtual void RequestFarmLayoutLibrary() noexcept = 0;
};
}
