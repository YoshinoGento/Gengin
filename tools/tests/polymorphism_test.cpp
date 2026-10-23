#include "farm/system/FarmToolBehavior.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/core/FarmGrid.h"
#include "scene/AbstractSceneFactory.h"
#include "scene/SceneViewportInput.h"
#include "scene/GamePlayScene.h"
#include "scene/TitleScene.h"
#include "command/CommandHistory.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct SceneProbe { int updates = 0, draws = 0, destroyed = 0; scene::ViewportPointerInput pointer{}; };
class ProbeScene : public BaseScene {
public:
    explicit ProbeScene(SceneProbe& probe) : probe_(probe) {}
    ~ProbeScene() override { ++probe_.destroyed; }
    void Initialize() override {}
    void Finalize() override {}
    void Update() override { ++probe_.updates; }
    void Draw() override { ++probe_.draws; }
private:
    SceneProbe& probe_;
};
class PointerScene final : public ProbeScene, public scene::ViewportInputTarget {
public:
    explicit PointerScene(SceneProbe& probe) : ProbeScene(probe), probe_(probe) {}
    SceneClipPlanes GetPostEffectClipPlanes() const noexcept override { return {0.5f, 250.0f}; }
    void SetViewportPointerInput(const scene::ViewportPointerInput& input) noexcept override { probe_.pointer = input; }
private:
    SceneProbe& probe_;
};
class ProbeFactory final : public AbstractSceneFactory {
public:
    explicit ProbeFactory(SceneProbe& probe) : probe_(probe) {}
    std::unique_ptr<BaseScene> CreateScene(const std::string& name) override {
        if (name == "pointer") return std::make_unique<PointerScene>(probe_);
        if (name == "basic") return std::make_unique<ProbeScene>(probe_);
        return {};
    }
private:
    SceneProbe& probe_;
};
class LifetimeBehavior final : public FarmToolBehavior {
public:
    explicit LifetimeBehavior(int& destroyed) : destroyed_(destroyed) {}
    ~LifetimeBehavior() override { ++destroyed_; }
    FarmToolPlan Plan(const FarmToolContext&) const noexcept override { return {}; }
private:
    int& destroyed_;
};
class ProbeCommand final : public IUndoableCommand {
public:
    ProbeCommand(int& value, int& destroyed) : value_(value), destroyed_(destroyed) {}
    ~ProbeCommand() override { ++destroyed_; }
    bool Execute() override { ++value_; return true; }
    bool Undo() override { --value_; return true; }
    std::string_view GetName() const noexcept override { return "probe"; }
private:
    int& value_;
    int& destroyed_;
};

void CheckScenesAndLifetime() {
    static_assert(std::has_virtual_destructor_v<BaseScene>);
    static_assert(std::has_virtual_destructor_v<AbstractSceneFactory>);
    static_assert(std::has_virtual_destructor_v<FarmToolBehavior>);
    static_assert(std::has_virtual_destructor_v<IUndoableCommand>);
    static_assert(std::has_virtual_destructor_v<editor::GamePlayEditorScene>);
    static_assert(std::has_virtual_destructor_v<scene::ViewportInputTarget>);
    static_assert(std::is_base_of_v<editor::GamePlayEditorScene, GamePlayScene>);
    static_assert(!std::is_base_of_v<editor::GamePlayEditorScene, TitleScene>);
    static_assert(std::is_base_of_v<scene::ViewportInputTarget, TitleScene>);
    static_assert(!std::is_abstract_v<GamePlayScene> && !std::is_abstract_v<TitleScene>);
    SceneProbe probe;
    std::unique_ptr<AbstractSceneFactory> factory = std::make_unique<ProbeFactory>(probe);
    auto basic = factory->CreateScene("basic");
    auto pointer = factory->CreateScene("pointer");
    Check(!factory->CreateScene("missing"), "unknown scene");
    Check(!dynamic_cast<scene::ViewportInputTarget*>(basic.get()), "unsupported capability");
    Check(basic->GetPostEffectClipPlanes().nearClip == 0.1f &&
        basic->GetPostEffectClipPlanes().farClip == 1000.0f, "default clip range changed");
    Check(pointer->GetPostEffectClipPlanes().nearClip == 0.5f &&
        pointer->GetPostEffectClipPlanes().farClip == 250.0f, "clip dispatch");
    auto* target = dynamic_cast<scene::ViewportInputTarget*>(pointer.get());
    Check(target != nullptr, "capability discovery");
    target->SetViewportPointerInput({{123.0f, 321.0f}, true, true, false, true});
    Check(probe.pointer.position.x == 123.0f && probe.pointer.focused && probe.pointer.pressed, "input dispatch");
    pointer->Update(); basic->Draw();
    Check(probe.updates == 1 && probe.draws == 1, "scene lifecycle dispatch");
    pointer.reset(); basic.reset();
    Check(probe.destroyed == 2, "scene virtual destruction");
    std::unique_ptr<scene::ViewportInputTarget> ownedCapability = std::make_unique<PointerScene>(probe);
    ownedCapability.reset();
    Check(probe.destroyed == 3, "multiple-inheritance virtual destruction");
    int destroyed = 0;
    std::unique_ptr<FarmToolBehavior> ownedBehavior = std::make_unique<LifetimeBehavior>(destroyed);
    ownedBehavior.reset();
    Check(destroyed == 1, "behavior virtual destruction");
    int value = 0;
    CommandHistory history;
    Check(history.Execute(std::make_unique<ProbeCommand>(value, destroyed)), "execute dispatch");
    Check(value == 1 && history.Undo() && value == 0 && history.Redo() && value == 1, "undo redo dispatch");
    history.Clear();
    Check(destroyed == 2, "command virtual destruction");
}

void CheckToolPlans() {
    farm::FarmRules rules;
    rules.initialCarrotSeedCount = 5;
    FarmCropQualitySystem quality; quality.Initialize(rules);
    FarmEconomySystem economy; economy.Initialize(rules);
    farm::FarmGrid grid; Check(grid.Initialize(1, 1), "grid init");
    FarmToolActionSystem tools; tools.Initialize(rules);
    const auto crop = farm::CropType::Carrot;
    const int initialSeeds = economy.GetSeedCount(crop);
    const int initialMoney = economy.GetMoney();
    for (auto tool : {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest}) {
        if (tool == FarmTool::Harvest) {
            auto mature = *grid.GetTile(0); mature.growth = 1.0f;
            Check(grid.SetTile(0, mature), "mature fixture");
        }
        const FarmToolBehavior* behavior = FindFarmToolBehavior(tool);
        Check(behavior && behavior == FindFarmToolBehavior(tool), "stable strategy borrow");
        const auto before = *grid.GetTile(0);
        const auto plan = behavior->Plan({before, crop, &economy, nullptr, quality, farm::FarmRules{}.wateringMoistureIncrement});
        const auto preview = tools.EvaluateTool(grid, tool, crop, &economy);
        if (!plan.result.Succeeded() || preview.status != plan.result.status) {
            std::cerr << "tool=" << static_cast<int>(tool) << " plan=" << static_cast<int>(plan.result.status)
                << " preview=" << static_cast<int>(preview.status) << " seeds=" << economy.GetSeedCount(crop) << '\n';
        }
        Check(plan.result.Succeeded() && preview.status == plan.result.status, "preview base dispatch");
        Check(grid.GetTile(0)->state == before.state && grid.GetTile(0)->moisture == before.moisture &&
            economy.GetMoney() == initialMoney, "planning mutated live state");
        const auto applied = tools.ApplyToolDetailed(grid, tool, crop, economy, 10);
        Check(applied.status == preview.status && applied.reward == preview.reward, "apply preview parity");
        const auto* after = grid.GetTile(0);
        Check(after->state == plan.after.state && after->crop == plan.after.crop &&
            after->moisture == plan.after.moisture && after->growth == plan.after.growth, "plan commit mismatch");
        if (tool == FarmTool::Seed) Check(economy.GetSeedCount(crop) == initialSeeds - 1, "seed consumption");
        if (tool == FarmTool::Harvest) Check(applied.harvestedTile.has_value() && economy.GetHarvestRecordCount() == 1, "harvest result");
        Check(tools.Undo(), "tool undo");
        Check(grid.GetTile(0)->state == before.state && grid.GetTile(0)->growth == before.growth, "undo tile mismatch");
        Check(tools.Redo(), "tool redo");
    }
    for (auto invalid : {FarmTool::BugNet, static_cast<FarmTool>(-1), static_cast<FarmTool>(999)}) {
        Check(!FindFarmToolBehavior(invalid), "unsupported tool acquired behavior");
        const auto historyCount = tools.GetHistory().GetUndoCount();
        Check(tools.ApplyToolDetailed(grid, invalid, crop, economy).status == FarmToolActionStatus::UnsupportedTool, "unsupported status");
        Check(tools.GetHistory().GetUndoCount() == historyCount, "unsupported history mutation");
    }
    Check(tools.EvaluateTool(grid, -1, FarmTool::Water, crop).status == FarmToolActionStatus::InvalidTarget, "invalid index");
    auto tile = *grid.GetTile(0); tile.moisture = 1.0f;
    Check(grid.SetTile(0, tile), "full water fixture");
    Check(tools.EvaluateTool(grid, FarmTool::Water, crop).status == FarmToolActionStatus::AlreadyWatered, "full water status");
    const FarmToolBehavior& water = *FindFarmToolBehavior(FarmTool::Water);
    for (float increment : {0.0f, -1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN()}) {
        Check(water.Plan({tile, crop, nullptr, nullptr, quality, increment}).result.status == FarmToolActionStatus::InvalidState, "invalid watering increment");
    }
}
}

int main() {
    try {
        CheckScenesAndLifetime();
        CheckToolPlans();
        std::cout << "PASS: polymorphic scene/capability/tool dispatch, virtual destruction, preview/commit/undo parity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
