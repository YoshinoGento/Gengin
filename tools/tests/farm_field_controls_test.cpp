#include "farm/system/FarmFieldInput.h"
#include "farm/system/FarmInputSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmWalkSystem.h"
#include "io/Input.h"
#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <span>
#include <utility>

namespace {
std::span<const InputKey> keys{};
void TestRightClick() {
    using Gesture = farm::FieldRightTap;
    using enum Gesture::Result;
    Gesture tap;
    const auto frame = [&](bool down, bool held, bool up, int ms, float x = 10.f,
        bool field = true, unsigned board = 1) {
        return tap.Sample(down, held, up, field, x, 10.f, board,
            Gesture::Clock::time_point{} + std::chrono::milliseconds(ms));
    };
    assert(frame(true,false,true,0) == Tap);
    assert(frame(false,false,true,1) == Pass);
    assert(frame(true,true,false,2) == Pending && frame(false,false,true,100,12) == Tap);
    for (int cancel = 0; cancel < 4; ++cancel) {
        assert(frame(true,true,false,200) == Pending);
        assert(frame(false,true,false,cancel == 0 ? 601 : 201, cancel == 1 ? 17.f : 10.f,
            cancel != 2, cancel == 3 ? 2 : 1) == Pass);
        assert(frame(false,false,true,602) == Pass);
    }
    assert(frame(true,false,true,700,10,false) == Pass);
    assert(frame(true,true,false,750) == Pending);
    tap = {};
    assert(frame(false,false,true,751) == Pass);

    FarmToolSystem bar;
    bar.SetTool(FarmTool::Water);
    bar.ToggleSeedKit();
    assert(bar.GetCurrentTool() == FarmTool::Seed && bar.WheelSelectsSeeds());
    bar.SetTool(FarmTool::Seed);
    assert(bar.WheelSelectsSeeds());
    bar.ToggleSeedKit();
    assert(bar.GetCurrentTool() == FarmTool::Water && !bar.WheelSelectsSeeds());
    bar.SelectNextTool();
    assert(bar.GetCurrentTool() == FarmTool::Seed && !bar.WheelSelectsSeeds());
    bar.SelectNextTool(); bar.ToggleSeedKit(); bar.ToggleSeedKit();
    assert(bar.GetCurrentTool() == FarmTool::Harvest);
    bar.Initialize(); bar.ToggleSeedKit(); bar.ToggleSeedKit();
    assert(bar.GetCurrentTool() == FarmTool::Hoe);
}
}

// Snapshot boundary: the tool, inventory, history and grid implementations are real.
bool Input::TriggerKey(InputKey key) const { return std::find(keys.begin(), keys.end(), key) != keys.end(); }
bool Input::PushKey(InputKey key) const { return TriggerKey(key); }
bool Input::TriggerGamepadButton(InputGamepadButton) const { return false; }

int main() {
    {
        farm::FarmGrid g;
        assert(g.Initialize(4,3) && g.SetSelectedIndex(0));
        FarmToolActionSystem tools;
        assert(tools.DigCanalAt(g,1) && !tools.DigCanalAt(g,1));
        assert(!tools.DigCanalAt(g,-1) && tools.GetHistory().GetUndoCount()==1);
        assert(g.GetSelectedIndex()==0 && g.GetTile(1)->feature==farm::FarmTileFeature::Canal);
        assert(tools.ToggleSelectedWaterSource(g) && !tools.DigCanalAt(g,0));
        assert(tools.Undo() && tools.Undo());
        farm::FarmWalkSystem walk;
        assert(!walk.Active() && walk.Enter(g,{{1,2,3},{.7f,0,0}}));
        assert(walk.Step(g,0,0,true,.016f)==-1);
        walk.Step(g,0,0,false,.016f);
        for (int i=0;i<40;++i) assert(walk.Step(g,0,0,true,.016f)==-1);
        assert(walk.Pose().bladeDepth==1.f && walk.Pose().furrow.tile==0);
        for (int i=0;i<4;++i) assert(walk.Step(g,1,0,true,.02f)==-1);
        assert(walk.Pose().furrow.endX>walk.Pose().furrow.startX);
        walk.Step(g,0,0,false,.02f);
        assert(walk.Pose().furrow.tile==-1 && tools.GetHistory().GetUndoCount()==0);
        for (int i=0;i<8;++i) walk.Step(g,-1,0,false,.02f);
        for (int i=0;i<24;++i) assert(walk.Step(g,-1,0,true,.02f)==-1); // Field boundary.
        int carved=0;
        const int expected[]={0,1,5};
        for (int target : {1,5}) for (int i=0;i<44;++i) {
            const int hit=walk.Step(g,0,0,true,.02f,target); // Mouse-only destination.
            if (hit<0) continue;
            assert(carved<3 && hit==expected[carved++] && tools.DigCanalAt(g,hit));
        }
        assert(carved==3 && tools.GetHistory().GetUndoCount()==3);
        assert(walk.Step(g,0,0,true,.02f)==-1 && walk.Pose().furrow.tile==-1);
        assert(tools.Undo() && g.GetTile(5)->feature==farm::FarmTileFeature::None);
        walk.Disarm();
        for (int i=0;i<10;++i) assert(walk.Step(g,1,0,true,.02f)==-1);
        assert(walk.Pose().bladeDepth==0.f);
        for (int i=0;i<150;++i) assert(walk.Step(g,-1,0,false,.02f)==-1);
        assert(walk.Tile(g)==4);
        assert(g.Initialize(2,2) && !walk.SameBoard(g));
        assert(walk.Step(g,1,0,true,1.f)==-1);
        assert(walk.Leave().position.z==3 && !walk.Active());
        std::cout<<"PASS: farm walk, turn, release, protected source, repeat digging and overview restore\n";
    }
    TestRightClick();
    for (const float dt:{1.f/120.f,1.f/30.f,.08f}) {
        farm::FarmGrid strip;
        assert(strip.Initialize(5,1));
        farm::FarmWalkSystem w;
        assert(w.Enter(strip,{}));
        w.Step(strip,0,0,false,dt);
        int last=-1;
        for (int i=0;i<600;++i) {
            const int hit=w.Step(strip,1,0,true,dt);
            if (hit<0) continue;
            assert(hit==last+1);
            if (hit==0) assert(w.X()>.45f && w.Pose().bladeDepth==1.f);
            last=hit;
        }
        assert(last==4);
    }
    std::cout<<"PASS: no stationary or airborne digging, uninterrupted keyboard path at three frame steps\n";
    farm::FieldToolWheel wheel;
    FarmToolSystem bar;
    const auto scroll = [&](int raw) {
        bar.CycleTools(wheel.Sample(raw));
        return bar.GetCurrentTool();
    };
    assert(scroll(-60) == FarmTool::Hoe && wheel.HasRemainder());
    assert(scroll(0) == FarmTool::Hoe && scroll(-60) == FarmTool::Water);
    assert(!wheel.HasRemainder() && scroll(-240) == FarmTool::Harvest);
    assert(scroll(-120) == FarmTool::Hoe && scroll(120) == FarmTool::Harvest);
    assert(scroll(60) == FarmTool::Harvest && scroll(-60) == FarmTool::Harvest && !wheel.HasRemainder());
    assert(scroll(-90) == FarmTool::Harvest);
    wheel = {}; // Losing the field route discards fractional input.
    assert(scroll(-30) == FarmTool::Harvest && scroll(-90) == FarmTool::Hoe);
    assert(scroll(480) == FarmTool::Hoe);
    assert(wheel.Sample(-119) == 0);
    assert(wheel.Sample((std::numeric_limits<int>::min)()) == 17895698);
    wheel = {};
    assert(wheel.Sample(119) == 0 && wheel.Sample((std::numeric_limits<int>::max)()) == -17895698);
    bar.SetTool(FarmTool::BugNet); bar.SelectPreviousTool(); assert(bar.GetCurrentTool() == FarmTool::Harvest);
    bar.SetTool(FarmTool::BugNet); bar.SelectNextTool(); assert(bar.GetCurrentTool() == FarmTool::Hoe);
    std::cout << "PASS: wheel detents, fractional carry/reset, forward/backward wrap and extreme deltas\n";

    farm::FieldClick click;
    const auto tap = [&](int index) { return click.Sample(index, 1, true, false, true); };
    assert(tap(5) == 5 && !click.Active());
    assert(click.Sample(5, 1, false, false, true) < 0);
    assert(click.Sample(5, 1, true, true, false) < 0);
    for (int i = 0; i < 120; ++i) assert(click.Sample(5, 1, false, true, false) < 0);
    assert(click.Sample(5, 1, false, false, true) == 5);
    for (int away : {-1, 6}) {
        assert(click.Sample(5, 1, true, true, false) < 0);
        assert(click.Sample(away, 1, false, true, false) < 0);
        assert(click.Sample(5, 1, false, false, true) < 0);
    }
    assert(tap(-1) < 0);
    assert(click.Sample(-1, 1, true, true, false) < 0);
    assert(click.Sample(5, 1, false, false, true) < 0);
    assert(click.Sample(5, 1, true, true, false) < 0);
    assert(click.Sample(5, 2, false, false, true) < 0);
    assert(click.Sample(5, 2, true, true, false) < 0);
    click = {}; // A frame routed to another screen drops the pending field press.
    assert(click.Sample(5, 2, false, false, true) < 0);

    Input input;
    FarmInputSystem controls;
    FarmInputContext context;
    context.currentDay = 3;
    farm::FarmRules rules;
    rules.initialCarrotSeedCount = 2;
    constexpr auto crop = farm::CropType::Carrot;
    struct Field {
        farm::FarmGrid grid;
        FarmToolSystem tool;
        FarmToolActionSystem actions;
        FarmEconomySystem economy;
        explicit Field(const farm::FarmRules& r) {
            assert(grid.Initialize(3, 3)); tool.Initialize(); actions.Initialize(r); economy.Initialize(r);
        }
    };
    Field kb(rules), mouse(rules);
    const auto equip = [&](std::initializer_list<InputKey> frame, FarmInputContext ctx) {
        keys = {frame.begin(), frame.size()};
        const bool handled = FarmInputSystem::SelectToolFromKeys(input,ctx,mouse.tool);
        keys = {};
        return handled;
    };
    assert(equip({InputKey::Num1},context)); // Explicitly keep the hoe instead of a focused seed bag.
    assert(equip({InputKey::Num2,InputKey::E},context) && mouse.tool.GetCurrentTool() == FarmTool::Water);
    assert(equip({InputKey::Q},context) && mouse.tool.GetCurrentTool() == FarmTool::Hoe);
    auto gated = context;
    gated.keyboardEnabled = false;
    assert(!equip({InputKey::Num3,InputKey::E},gated));
    gated = context; gated.cameraDragActive = true; gated.directToolSelectionEnabled = false;
    assert(!equip({InputKey::Num4,InputKey::Q},gated));
    assert(mouse.actions.GetHistory().GetUndoCount() == 0 && mouse.economy.GetSeedCount(crop) == 2);
    const auto keyboard = [&](std::initializer_list<InputKey> frame) {
        keys = {frame.begin(), frame.size()};
        const auto result = controls.Update(input, context, kb.grid, kb.tool, crop, kb.economy, kb.actions);
        keys = {};
        return result;
    };
    assert(kb.grid.SetSelectedIndex(4));
    for (const auto [key, expected] : {std::pair{InputKey::ArrowUp, 7}, {InputKey::ArrowRight, 8},
            {InputKey::ArrowDown, 5}, {InputKey::ArrowLeft, 4}}) {
        assert(keyboard({key}).selectionChanged && kb.grid.GetSelectedIndex() == expected);
    }
    assert(!keyboard({InputKey::ArrowUp, InputKey::ArrowDown}).selectionChanged);
    for (const auto [key, tool] : {std::pair{InputKey::Num1, FarmTool::Hoe}, {InputKey::Num2, FarmTool::Water},
            {InputKey::Num3, FarmTool::Seed}, {InputKey::Num4, FarmTool::Harvest}}) {
        if (tool == FarmTool::Harvest) {
            for (auto* field : {&kb, &mouse}) {
                auto ripe = *field->grid.GetTile(4);
                ripe.growth = 1.0f;
                assert(field->grid.SetTile(4, ripe));
            }
        }
        (void)keyboard({key});
        const auto keyResult = keyboard({InputKey::Enter}).toolAction;
        assert(keyResult.Succeeded());
        mouse.tool.SetTool(tool);
        if (tool == FarmTool::Seed) {
            const int seeds = mouse.economy.GetSeedCount(crop);
            assert(click.Sample(4, 1, true, true, false) < 0);
            assert(click.Sample(5, 1, false, false, true) < 0 && mouse.economy.GetSeedCount(crop) == seeds);
        }
        assert(mouse.grid.SetSelectedIndex(tap(4)));
        const auto clicked = mouse.actions.ApplyToolDetailed(mouse.grid, mouse.tool.GetCurrentTool(), crop, mouse.economy, context.currentDay);
        assert(clicked.status == keyResult.status && mouse.grid.GetTile(4)->state == kb.grid.GetTile(4)->state);
        assert(mouse.grid.GetTile(4)->moisture == kb.grid.GetTile(4)->moisture);
        assert(mouse.economy.GetSeedCount(crop) == kb.economy.GetSeedCount(crop));
        assert(mouse.actions.GetHistory().GetUndoCount() == kb.actions.GetHistory().GetUndoCount());
    }
    assert(mouse.economy.GetHarvestRecordCount() == 1 && kb.economy.GetHarvestRecordCount() == 1);
    std::cout << "PASS: single release, hold, drag, invalid hits, board/focus changes, arrows and four-tool parity\n";
}
