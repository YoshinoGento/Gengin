#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmObservationView.h"
#include "farm/ui/FarmPlayFlowView.h"
#include "farm/ui/FarmFieldActionView.h"
#include "farm/ui/FarmCameraView.h"
#include "farm/ui/FarmQualityView.h"
#include "farm/ui/FarmTerrainView.h"
#include "farm/ui/FarmHotbarView.h"
#include "farm/ui/FarmMenuView.h"
#include "farm/system/FarmOverviewCamera.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace farmui;
bool layoutValid = true;
std::array<bool,kLabels.size()> reportedLabels{};

bool Separated(Rect a, Rect b) {
    return a.x+a.width <= b.x || b.x+b.width <= a.x ||
        a.y+a.height <= b.y || b.y+b.height <= a.y;
}

Vector2 Center(Rect rect) {
    return {rect.x+rect.width/2, rect.y+rect.height/2};
}

void AssertReadable(const Item& item) {
    if (item.icon != HUDIcon::None) {
        assert(IsValidIcon(item.icon));
        assert(item.rect.width > 12 && item.rect.height > 12);
        return; // Icon labels are captions, not text inside the button.
    }
    const float space = item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
    const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
    const float scale = (std::min)(1.f,space/uv.width);
    // The atlas has transparent vertical padding; it is not the visible glyph height.
    const bool fits = scale >= 20.f/26.f &&
        (item.value.empty() || item.valueOffset+14*(item.value.size()-1)+24 <= item.rect.width);
    if (!fits && !reportedLabels[static_cast<std::size_t>(item.label)]) {
        std::cerr << "Label " << static_cast<int>(item.label) << " requires " << uv.width*20.f/26.f
            << "px but has " << space << "px at " << item.rect.x << ',' << item.rect.y << '\n';
        reportedLabels[static_cast<std::size_t>(item.label)] = true;
    }
    layoutValid &= fits;
}

void AssertItemsDoNotOverlap(const View& view) {
    assert(view.count <= View::kCapacity);
    for (std::size_t i = 0; i < view.count; ++i) {
        const auto& a = view.items[i].rect;
        assert(a.width > 0 && a.height > 0);
        assert(a.x >= 0 && a.y >= 0 && a.x+a.width <= 1280 && a.y+a.height <= 720);
        for (std::size_t j = i+1; j < view.count; ++j) {
            if (!Separated(a, view.items[j].rect))
                std::cerr << "Overlap: items " << i << " and " << j << " (labels "
                    << static_cast<int>(view.items[i].label) << ", "
                    << static_cast<int>(view.items[j].label) << ")\n";
            assert(Separated(a, view.items[j].rect));
        }
    }
}

void AssertHalfOpenControl(const Item& item) {
    View isolated;
    isolated.items[0] = item;
    isolated.count = 1;
    const Rect r = item.rect;
    const float right = r.x+r.width, bottom = r.y+r.height;
    const float insideX = std::nextafter(right, r.x), insideY = std::nextafter(bottom, r.y);
    const auto expected = item.enabled ? item.request.action : Action::None;
    for (const auto p : {Vector2{r.x,r.y}, Vector2{insideX,r.y},
        Vector2{r.x,insideY}, Vector2{insideX,insideY}, Center(r)}) {
        assert(r.Contains(p) && isolated.Covers(p));
        const auto hit = isolated.Hit(p);
        assert(hit.action == expected);
        if (expected != Action::None) {
            assert(hit.argument == item.request.argument);
            assert(hit.inventoryGeneration == item.request.inventoryGeneration);
        }
    }
    for (const auto p : {Vector2{right,r.y}, Vector2{r.x,bottom}, Vector2{right,bottom},
        Vector2{std::nextafter(r.x,-INFINITY),r.y}, Vector2{r.x,std::nextafter(r.y,-INFINITY)},
        Vector2{std::numeric_limits<float>::quiet_NaN(),r.y}, Vector2{r.x,INFINITY}}) {
        assert(!r.Contains(p) && !isolated.Covers(p));
        assert(isolated.Hit(p).action == Action::None);
    }
}

void AssertPanelBounds(const View& view, Rect r, bool covered = true) {
    for (const auto p : {Vector2{r.x,r.y},
        Vector2{std::nextafter(r.x+r.width,r.x),std::nextafter(r.y+r.height,r.y)}, Center(r)}) {
        assert(view.Covers(p) == covered);
        assert(view.Hit(p).action == Action::None);
    }
    for (const auto p : {Vector2{r.x+r.width,r.y}, Vector2{r.x,r.y+r.height},
        Vector2{std::nextafter(r.x,-INFINITY),r.y}, Vector2{r.x,std::nextafter(r.y,-INFINITY)}})
        assert(!view.Covers(p));
}

void TestIconAtlasIndices() {
    constexpr std::array icons{
        HUDIcon::Hoe, HUDIcon::WateringCan, HUDIcon::SeedBag, HUDIcon::HarvestBasket,
        HUDIcon::Carrot, HUDIcon::Tomato, HUDIcon::Pumpkin, HUDIcon::WaterDrop,
        HUDIcon::Sprout, HUDIcon::Calendar, HUDIcon::Coin, HUDIcon::Terrain,
        HUDIcon::Pause, HUDIcon::Play, HUDIcon::Overview, HUDIcon::Follow,
        HUDIcon::Menu, HUDIcon::Shop, HUDIcon::SeedCarrot, HUDIcon::SeedTomato,
        HUDIcon::SeedPumpkin, HUDIcon::Rank, HUDIcon::Canal, HUDIcon::Source,
        HUDIcon::Back, HUDIcon::Check, HUDIcon::Undo, HUDIcon::Redo,
        HUDIcon::Plus, HUDIcon::Minus, HUDIcon::Close, HUDIcon::Inventory};
    static_assert(icons.size() == 32 && View::kCapacity == 40);
    assert(kHUDIconSize == 64);
    for (std::size_t i = 0; i < icons.size(); ++i) {
        assert(static_cast<int>(icons[i]) == static_cast<int>(i) && IsValidIcon(icons[i]));
        const auto uv = IconUV(icons[i]);
        assert(uv.x == static_cast<float>(i%8)*64 && uv.y == static_cast<float>(i/8)*64);
        assert(uv.x+64 <= 512 && uv.y+64 <= 256);
    }
    for (int invalid : {-2,-1,32,33,(std::numeric_limits<int>::max)()}) {
        const auto icon = static_cast<HUDIcon>(invalid);
        assert(!IsValidIcon(icon));
        assert(IconUV(icon).x == 0 && IconUV(icon).y == 0);
    }
}

void TestHotbar() {
    constexpr std::array toolLabels{Label::Hoe,Label::Water,Label::Seed,Label::Harvest};
    constexpr std::array toolIcons{HUDIcon::Hoe,HUDIcon::WateringCan,HUDIcon::SeedBag,HUDIcon::HarvestBasket};
    constexpr std::array cropLabels{Label::Carrot,Label::Tomato,Label::Pumpkin};
    constexpr std::array cropIcons{HUDIcon::SeedCarrot,HUDIcon::SeedTomato,HUDIcon::SeedPumpkin};
    static_assert(farm::kPlayableCropCount == 3);
    for (bool enabled : {false,true}) for (int selectedTool : {-1,0,1,2,3,4})
    for (auto selectedCrop : {farm::CropType::None,farm::CropType::TestCrop,farm::CropType::Carrot,
        farm::CropType::Tomato,farm::CropType::Pumpkin})
    for (int seeds : {-1,0,7,(std::numeric_limits<int>::max)()}) {
        FarmHUDViewData state;
        state.currentToolIndex = selectedTool;
        state.selectedSeedCrop = selectedCrop;
        state.cropSeedCounts = {999,seeds,seeds,seeds};
        View bar;
        assert(!bar.hotbarVisible);
        BuildHotbarView(bar,state,enabled);
        assert(bar.hotbarVisible && bar.count == 7);
        AssertItemsDoNotOverlap(bar);
        int selectedTools = 0, selectedCrops = 0;
        for (std::size_t i = 0; i < bar.count; ++i) {
            const auto& item = bar.items[i];
            const bool tool = i < 4;
            const auto index = tool ? i : i-4;
            const auto expectedRect = tool ? HUDLayout::ToolSlot(static_cast<int>(index)) : HUDLayout::SeedSlot(static_cast<int>(index));
            assert(item.rect.x == expectedRect.x && item.rect.y == expectedRect.y);
            assert(item.rect.width == expectedRect.width && item.rect.height == expectedRect.height);
            assert(item.label == (tool ? toolLabels[index] : cropLabels[index]));
            assert(item.icon == (tool ? toolIcons[index] : cropIcons[index]));
            assert(item.request.action == (tool ? Action::Tool : Action::Crop));
            assert(item.request.argument == static_cast<int>(tool ? index : index+1));
            assert(item.enabled == enabled);
            assert(item.value == (tool ? "" : std::to_string((std::max)(0,seeds))));
            if (!tool) {
                const float glyphWidth = 14*static_cast<float>(item.value.size())+10;
                const float scale = (std::min)(0.58f,(item.rect.width-10)/glyphWidth);
                assert(glyphWidth*scale+9 <= item.rect.width+0.001f);
                assert(36*scale+3 <= item.rect.height);
            }
            const bool selected = tool ? selectedTool == static_cast<int>(index) :
                selectedCrop == farm::PlayableCrop(static_cast<int>(index));
            assert(item.selected == selected);
            if (tool) selectedTools += item.selected; else selectedCrops += item.selected;
            AssertReadable(item);
            AssertHalfOpenControl(item);
            assert(item.rect.x >= HUDLayout::tools.x && item.rect.y >= HUDLayout::tools.y);
            assert(item.rect.x+item.rect.width <= HUDLayout::tools.x+HUDLayout::tools.width);
            assert(item.rect.y+item.rect.height <= HUDLayout::tools.y+HUDLayout::tools.height);
            assert(bar.NextActionable(static_cast<int>(i),1) == (enabled ? static_cast<int>((i+1)%7) : -1));
            assert(bar.NextActionable(static_cast<int>(i),-1) == (enabled ? static_cast<int>((i+6)%7) : -1));
        }
        assert(selectedTools == (selectedTool >= 0 && selectedTool < 4 ? 1 : 0));
        assert(selectedCrops == (farm::ToCropSlot(selectedCrop) >= 1 ? 1 : 0));
        assert(bar.Hit({452,650}).action == Action::None && !bar.Covers({452,650}));
        bar.compactHUD = true;
        assert(bar.Covers({452,650}) && bar.Hit({452,650}).action == Action::None);
    }
    for (int available : {0,1,6,7}) {
        View full;
        while (full.count < View::kCapacity-static_cast<std::size_t>(available)) full.Add(Label::Ready,{});
        const auto prefix = full.count;
        BuildHotbarView(full,{},true);
        assert(full.count == View::kCapacity);
        if (prefix > 0) assert(full.items[prefix-1].icon == HUDIcon::None);
        const auto last = full.items.back();
        full.IconButton(Label::Menu,HUDIcon::Menu,{}, {Action::Menu},true,true,"999");
        assert(full.count == View::kCapacity && full.items.back().icon == last.icon);
        assert(full.items.back().request.action == last.request.action && full.items.back().value == last.value);
    }
    std::cout << "PASS: hotbar 4 tools/3 crops, zero seeds, selection, navigation, exact bounds, capacity\n";
}

void TestCompactPanelCoverage() {
    for (bool compact : {false,true}) for (bool hotbar : {false,true})
    for (bool selected : {false,true}) for (bool goal : {false,true}) {
        View view;
        view.compactHUD = compact;
        view.hotbarVisible = hotbar;
        view.selectedTile = selected;
        view.goalVisible = goal;
        for (const auto panel : {HUDLayout::status,HUDLayout::economy})
            AssertPanelBounds(view,panel,compact);
        AssertPanelBounds(view,HUDLayout::tools,compact && hotbar);
        AssertPanelBounds(view,HUDLayout::selected,compact && selected);
        AssertPanelBounds(view,HUDLayout::goal,compact && goal);
        for (const auto freed : {Vector2{300,140},Vector2{900,200},Vector2{500,550},
            Vector2{390,548},Vector2{500,510},Vector2{1100,450},Vector2{1100,550},Vector2{640,300}})
            assert(!view.Covers(freed) && view.Hit(freed).action == Action::None);
        view.modal = true;
        assert(view.Covers({640,300}) && view.Hit({640,300}).action == Action::None);
    }
    for (bool enabled : {false,true}) for (bool selected : {false,true}) for (bool compact : {false,true}) {
        for (int i = 0; i < 4; ++i) {
            View view;
            FarmHUDViewData state; state.currentToolIndex = i;
            BuildHotbarView(view,state,enabled);
            view.compactHUD = compact;
            view.items[static_cast<std::size_t>(i)].selected = selected;
            const auto box = IconCaptionRect(view.items[static_cast<std::size_t>(i)]);
            View withoutCaption = view;
            withoutCaption.items[static_cast<std::size_t>(i)].selected = false;
            // The caption is presentation, never an implicit tool request.
            for (const auto p : {Center(box),Vector2{box.x,box.y},
                Vector2{std::nextafter(box.x+box.width,box.x),std::nextafter(box.y+box.height,box.y)},
                Vector2{box.x+box.width,box.y},Vector2{box.x,box.y+box.height},
                Vector2{std::nextafter(box.x,-INFINITY),box.y},Vector2{box.x,std::nextafter(box.y,-INFINITY)}}) {
                assert(view.Covers(p) == (withoutCaption.Covers(p) || (compact && selected && box.Contains(p))));
                assert(view.Hit(p).action == Action::None);
            }
        }
    }
    std::cout << "PASS: conditional selected/goal panels, freed HUD regions, selected-tool caption consumption\n";
}

void TestHotbarVisibilityAcrossCropPopup() {
    for (bool enabled : {false,true}) {
        View view;
        for (int cycle = 0; cycle < 2; ++cycle) {
            // BuildView starts fresh while the crop pie owns selection input.
            view = {};
            view.compactHUD = true;
            view.selectedTile = view.goalVisible = true;
            view.IconButton(Label::Menu,HUDIcon::Menu,HUDLayout::UtilitySlot(5),{Action::Menu});
            assert(!view.hotbarVisible && !view.modal);
            AssertPanelBounds(view,HUDLayout::tools,false);
            for (int y = 620; y < 696; ++y) for (int x = 380; x < 900; ++x) {
                const Vector2 point{static_cast<float>(x),static_cast<float>(y)};
                assert(!view.Covers(point) && view.Hit(point).action == Action::None);
            }
            // At the maximum allowed popup center (640,500), the down/Pumpkin
            // panel spans (553,578)-(727,704), through the old toolbar area.
            for (const auto point : {Vector2{640,578},Vector2{640,620},Vector2{640,650},Vector2{640,703}})
                assert(!view.Covers(point) && view.Hit(point).action == Action::None);
            assert(view.Covers(Center(HUDLayout::selected)) && view.Covers(Center(HUDLayout::goal)));
            assert(view.Hit(Center(HUDLayout::UtilitySlot(5))).action == Action::Menu);
            assert(!view.Covers({640,590}));

            FarmHUDViewData state;
            state.currentToolIndex = 0;
            BuildHotbarView(view,state,enabled);
            assert(view.hotbarVisible && view.count == 8);
            for (const auto point : {Vector2{380,620},Vector2{452,650},Vector2{899,695}})
                assert(view.Covers(point) && view.Hit(point).action == Action::None);
            assert(!view.Covers({900,620}) && !view.Covers({380,696}));
            const auto tool = view.Hit(Center(HUDLayout::ToolSlot(0)));
            assert(tool.action == (enabled ? Action::Tool : Action::None));
            if (enabled) assert(tool.argument == 0);
            assert(view.Covers({640,590}) && view.Hit({640,590}).action == Action::None);
        }
    }
    std::cout << "PASS: hidden hotbar frees crop-popup bottom area; rebuilding restores controls and caption\n";
}

void TestComposedHUD() {
    for (bool selected : {false,true}) for (bool goal : {false,true}) for (bool feedback : {false,true})
    for (bool enabled : {false,true}) for (bool player : {false,true}) {
        View view;
        view.compactHUD = true; view.selectedTile = selected; view.goalVisible = goal;
        FarmHUDViewData state;
        state.currentToolIndex = 0;
        state.selectedSeedCrop = farm::CropType::Carrot;
        BuildHotbarView(view,state,enabled);
        BuildPlayQuickView(view,false,enabled,enabled);
        BuildCameraView(view,true,player);
        BuildWaterGuidanceView(view,{selected,false,farm::FarmWaterStatus::Available,FarmWaterAdvice::Water},enabled,true);
        BuildPlayCropStatusView(view,feedback,3,farm::CropType::Pumpkin,nullptr);
        FarmToolActionResult evaluation; evaluation.status = FarmToolActionStatus::Applied;
        BuildFieldActionView(view,evaluation,false,!enabled);
        view.IconButton(Label::Menu,HUDIcon::Menu,HUDLayout::UtilitySlot(5),{Action::Menu});
        view.IconButton(Label::ShopTitle,HUDIcon::Shop,HUDLayout::UtilitySlot(0),{Action::OpenSeedShop},enabled);
        view.IconButton(Label::HarvestDisplay,HUDIcon::Inventory,HUDLayout::UtilitySlot(1),{Action::HarvestDisplay},enabled);
        AssertItemsDoNotOverlap(view);
        std::size_t panelsUsed = 3+(selected ? 1 : 0); // Hotbar, field actions, one caption, optional water guidance.
        std::size_t iconsUsed = 0, digitsUsed = 0;
        for (std::size_t i = 0; i < view.count; ++i) {
            const auto& item = view.items[i];
            AssertReadable(item);
            AssertHalfOpenControl(item);
            if (IsValidIcon(item.icon)) {
                ++iconsUsed;
                panelsUsed += 2+(item.value.empty() ? 0 : 1);
            } else if (item.request.action != Action::None) ++panelsUsed;
            digitsUsed += item.value.size();
        }
        assert(panelsUsed <= 84 && iconsUsed <= 40 && digitsUsed <= 160);
        const std::array panels{HUDLayout::status,HUDLayout::economy,HUDLayout::tools,HUDLayout::selected,
            HUDLayout::goal,HUDLayout::waterGuidance,HUDLayout::fieldActions,View::kFeedbackPanel};
        const std::array active{true,true,view.hotbarVisible,selected,goal,selected,true,feedback};
        const auto frame = farm::kFarmOverviewFrame;
        const Rect safe{frame.left*1280,frame.top*720,(frame.right-frame.left)*1280,(frame.bottom-frame.top)*720};
        for (std::size_t i = 0; i < panels.size(); ++i)
            for (std::size_t j = i+1; j < panels.size(); ++j) assert(Separated(panels[i],panels[j]));
        for (const auto panel : panels) assert(Separated(safe,panel));
        for (std::size_t i = 0; i < view.count; ++i) for (std::size_t j = 0; j < panels.size(); ++j) {
            if (!active[j]) continue;
            const auto panel = panels[j];
            const auto r = view.items[i].rect;
            const bool contained = r.x >= panel.x && r.y >= panel.y &&
                r.x+r.width <= panel.x+panel.width && r.y+r.height <= panel.y+panel.height;
            assert(contained || Separated(r,panel));
        }
        for (const auto freed : {Vector2{300,140},Vector2{900,200},Vector2{500,550},Vector2{1100,550}})
            assert(!view.Covers(freed));
    }
    std::cout << "PASS: composed HUD controls and panel non-overlap across visibility/enabled states\n";
}

void TestCaptionLayout() {
    View view;
    FarmHUDViewData state;
    BuildHotbarView(view,state,true);
    BuildCameraView(view,false,true);
    BuildPlayQuickView(view,false,true);
    BuildWaterGuidanceView(view,{true,false,farm::FarmWaterStatus::Available,FarmWaterAdvice::Water},true,true);
    BuildFieldActionView(view,{},false,false);
    view.IconButton(Label::Menu,HUDIcon::Menu,HUDLayout::UtilitySlot(5),{Action::Menu});
    view.IconButton(Label::ShopTitle,HUDIcon::Shop,HUDLayout::UtilitySlot(0),{Action::OpenSeedShop});
    view.IconButton(Label::HarvestDisplay,HUDIcon::Inventory,HUDLayout::UtilitySlot(1),{Action::HarvestDisplay});
    for (std::size_t i = 0; i < view.count; ++i) {
        const auto& item = view.items[i];
        if (!IsValidIcon(item.icon)) continue;
        const auto caption = IconCaptionRect(item);
        assert(caption.width > 0 && caption.height > 0);
        assert(caption.x >= 0 && caption.y >= 0 && caption.x+caption.width <= 1280 && caption.y+caption.height <= 720);
        for (std::size_t j = 0; j < view.count; ++j) {
            if (!Separated(caption,view.items[j].rect))
                std::cerr << "Caption for label " << static_cast<int>(item.label)
                    << " overlaps label " << static_cast<int>(view.items[j].label) << " at "
                    << caption.x << ',' << caption.y << " size " << caption.width << 'x' << caption.height << '\n';
            layoutValid &= Separated(caption,view.items[j].rect);
        }
        if (item.request.action == Action::Tool || item.request.action == Action::Crop) {
            if (!Separated(caption,HUDLayout::tools))
                std::cerr << "Hotbar caption overlaps tools panel: bottom=" << caption.y+caption.height
                    << ", panel top=" << HUDLayout::tools.y << '\n';
            layoutValid &= Separated(caption,HUDLayout::tools);
        }
    }
}
}

int main() {
    for (int page=0;page<kMenuPageCount;++page) {
        for (int flags=0;flags<64;++flags) {
            MenuOverviewState state;
            state.page=page; state.unlocked=(flags&1)!=0;
            state.canSave=state.canSaveCopy=state.unlocked;
            state.dirty=(flags&2)!=0; state.documents=(flags&4) ? 3 : 0;
            state.documentIndex=state.documents ? 1 : 0;
            state.comparing=(flags&8)!=0; state.canCompare=state.unlocked&&!state.comparing;
            state.following=(flags&16)!=0; state.contestSeason=(flags&32)!=0;
            state.money=999999; state.seeds=9999; state.maximumHeight=5;
            state.compareA=123; state.compareB=-1;
            state.suppliedCanals=123; state.suppliedTiles=456;
            View menu;
            BuildMenuShell(menu,page,false,false,false,Label::Ready);
            BuildMenuOverview(menu,state);
            AssertItemsDoNotOverlap(menu);
            assert(menu.menuShell && menu.modal && menu.count<View::kCapacity);
            assert(menu.Covers({10,10}) && menu.Hit({10,10}).action==Action::None);
            for (std::size_t i=0;i<menu.count;++i) {
                AssertReadable(menu.items[i]);
                AssertHalfOpenControl(menu.items[i]);
            }
            const int first=menu.NextActionable(-1,1);
            int cursor=first, count=0;
            do {
                assert(cursor>=0 && menu.items[cursor].enabled && menu.items[cursor].request.action!=Action::None);
                cursor=menu.NextActionable(cursor,1); ++count;
                assert(count<=static_cast<int>(menu.count));
            } while(cursor!=first);
            assert(menu.NextActionable(menu.NextActionable(first,-1),1)==first);
            if (page==2) assert((menu.Hit(Center(MenuLayout::Cell(1,308))).action==Action::Load)==(state.documents>0));
        }
    }
    for (int page=0;page<15;++page) {
        View confirmation;
        BuildMenuShell(confirmation,page,true,true,false,Label::Failure);
        assert(confirmation.NextActionable(-1,1)==-1);
        AssertItemsDoNotOverlap(confirmation);
        for (std::size_t i=0;i<confirmation.count;++i) AssertReadable(confirmation.items[i]);
    }
    for (const int volume : {0,40,100}) {
        title::AudioSettingsFrame frame;
        frame.open=true; frame.volume.fill(volume); frame.saveFailed=true;
        View audio;
        BuildMenuAudioView(audio,frame);
        AssertItemsDoNotOverlap(audio);
        assert(audio.modal && audio.audioSettings && audio.Covers({1,1}));
        assert(audio.NextActionable(-1,1)==-1); // Shared settings System consumes these inputs.
        assert(audio.audioFrame.volume==frame.volume);
        for (std::size_t i=0;i<audio.count;++i) AssertReadable(audio.items[i]);
    }
    std::cout << "PASS: 320 menu layouts, modal shell, audio view, keyboard navigation\n";
    for (int child=0;child<5;++child) {
        View detail;
        BuildMenuShell(detail,5+child,false,false,false,Label::Ready);
        if (child==0) BuildStageGuideView(detail,{});
        else if (child==1) BuildQualityView(detail,{}, {},0,false);
        else if (child==2) BuildHarvestInventoryView(detail,{});
        else if (child==3) BuildContestJudgeView(detail,{});
        else BuildContestResultsView(detail,{},{});
        AssertItemsDoNotOverlap(detail);
        assert(detail.count<View::kCapacity);
        for (std::size_t i=0;i<detail.count;++i) AssertReadable(detail.items[i]);
    }
    std::cout << "PASS: menu shell composed with 5 existing child views\n";
    std::cout << std::unitbuf;
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace farmui;
    const auto separated = [](Rect a, Rect b) {
        return a.x+a.width<=b.x || b.x+b.width<=a.x || a.y+a.height<=b.y || b.y+b.height<=a.y;
    };
    TestIconAtlasIndices();
    TestHotbar();
    TestCompactPanelCoverage();
    TestHotbarVisibilityAcrossCropPopup();
    TestComposedHUD();
    for (const auto grade : {FarmContestRating::Unrated,FarmContestRating::D,FarmContestRating::C,
        FarmContestRating::B,FarmContestRating::A,FarmContestRating::S}) {
        FarmContestSeasonSummary summary{};
        summary.valid=true; summary.finalized=true; summary.rating=grade;
        summary.submitted=grade==FarmContestRating::Unrated ? 0 : 3;
        summary.nextRating=grade==FarmContestRating::Unrated || grade==FarmContestRating::S ?
            FarmContestRating::Unrated : FarmContestRating::S;
        summary.pointsToNextRating=120;
        View result;
        BuildSeasonEndView(result,{},summary);
        bool found=false;
        for(std::size_t i=0;i<result.count;++i) {
            const auto& item=result.items[i];
            if(item.label==Label::SeasonRating || item.label==Label::SeasonUnrated) {
                found=true; assert(item.value==FarmContestRatingText(grade));
                assert((item.label==Label::SeasonUnrated)==(grade==FarmContestRating::Unrated));
            }
            const float space=item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=item.rect.width);
            for(std::size_t j=i+1;j<result.count;++j) assert(separated(item.rect,result.items[j].rect));
        }
        assert(found);
    }
    {
        FarmEconomySystem::ContestResults results{};
        View end; BuildSeasonEndView(end,results,FarmContestSeasonSystem::Evaluate(31,results));
        assert(end.modal && end.Hit({900,150}).action==Action::FlowRecords);
        assert(end.Hit({400,640}).action==Action::FlowContinue);
        assert(end.Hit({800,640}).action==Action::Restart);
        for(std::size_t i=0;i<end.count;++i) {
            const auto& a=end.items[i];
            assert(a.rect.y+a.rect.height<=688);
            for(std::size_t j=i+1;j<end.count;++j) assert(separated(a.rect,end.items[j].rect));
        }
        for(int mode : {0,1,2}) {
            View entry; FarmHUDViewData data; data.contestSeason=mode==1; data.freeFarming=mode==2;
            BuildPlayFlowView(entry,FarmPlayFlow::Phase::Briefing,data);
            const auto change=entry.Hit({300,660});
            assert(change.action==Action::ChangePlayMode && change.argument==(mode==2 ? 1 : 2));
            if (mode==2) assert(entry.items[0].label==Label::FreeMode);
            for(std::size_t i=0;i<entry.count;++i) {
                const auto& a=entry.items[i];
                const float space=a.value.empty() ? a.rect.width-20 : a.valueOffset-30;
                assert(space/kLabels[static_cast<std::size_t>(a.label)].width>=20.f/26.f);
                for(std::size_t j=i+1;j<entry.count;++j) assert(separated(a.rect,entry.items[j].rect));
            }
        }
    }
    for(int day : {10,20,30}) {
        View view; BuildContestDayNoticeView(view,day);
        assert(view.modal && view.count==7);
        assert(view.Hit({400,445}).action==Action::ContestDayReview);
        assert(view.Hit({800,445}).action==Action::ContestDayPrepare);
        assert(view.Hit({400,520}).action==Action::ContestDayResume);
        assert(view.Hit({400,445}).argument==day);
        assert(view.Covers({500,300}) && view.Hit({500,300}).action==Action::None);
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
    }
    for (int message=0; message<=static_cast<int>(FarmHUDFeedback::InsufficientMoney); ++message)
        for (int protectedCount : {0, 2, 2147483647})
            for (auto crop : {farm::CropType::None, farm::CropType::TestCrop, farm::CropType::Carrot})
                for (bool planted : {false, true}) {
        const bool feedback = static_cast<FarmHUDFeedback>(message) != FarmHUDFeedback::None;
        const FarmCropSizeResult size{1.5f, true};
        View view;
        BuildPlayQuickView(view, false, true);
        BuildPlayCropStatusView(view, feedback, protectedCount, crop, planted ? &size : nullptr);
        const std::size_t metrics = (planted ? 1u : 0u) + (protectedCount > 0 ? 1u : 0u) +
            (crop != farm::CropType::None ? 1u : 0u);
        assert(view.feedback == feedback && view.count == 5 + (feedback ? 0 : metrics));
        const auto panel = View::kFeedbackPanel;
        assert(panel.x>=0 && panel.y>=0 && panel.x+panel.width<=1280 && panel.y+panel.height<=720);
        for (const auto& other : View::kFarmHudPanels) assert(separated(panel, other));
        for (std::size_t i=0; i<view.count; ++i) {
            if (feedback) assert(separated(panel, view.items[i].rect));
            for (std::size_t j=i+1; j<view.count; ++j) assert(separated(view.items[i].rect,view.items[j].rect));
        }
        if (feedback) {
            assert(view.Covers({panel.x,panel.y}));
            assert(view.Covers({panel.x+panel.width-1,panel.y+panel.height-1}));
            assert(!view.Covers({panel.x+panel.width,panel.y+panel.height}));
            assert(view.Hit({panel.x+10,panel.y+10}).action==Action::None);
            assert(view.Hit({280,40}).action==Action::TerrainField);
            assert(view.Hit({350,40}).action==Action::Pause);
        }
        // Each refresh starts a new View, restoring current data rather than cached pre-toast values.
        view = {};
        BuildPlayCropStatusView(view, false, 3, farm::CropType::Carrot, &size);
        assert(!view.feedback && view.count==3);
        assert(view.items[0].label==Label::SizeForecast && view.items[0].value=="1.50x");
        assert(view.items[1].label==Label::ProtectedCropCount && view.items[1].value=="3");
        assert(view.items[2].label==Label::ContestCarrot);
    }
    for (const auto issue : {FarmContestJudgeIssue::None, FarmContestJudgeIssue::NoReservation,
        FarmContestJudgeIssue::InvalidRecord, FarmContestJudgeIssue::InvalidRules}) {
        FarmContestJudgeResult result; result.issue=issue; result.crop=farm::CropType::Carrot;
        result.recordedQuality=100; result.recordedSize=8; result.qualityPoints=60; result.sizePoints=40; result.totalPoints=100;
        View view; BuildContestJudgeView(view,result);
        assert(view.Hit({800,150}).action==Action::ContestPreview && view.Hit({800,150}).argument==1);
        assert(view.Hit({800,200}).action==Action::HarvestInventory);
        std::size_t glyphs=0;
        bool total=false;
        for (std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            total |= item.label==Label::ContestTotal;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) {
                const auto& other=view.items[j].rect;
                assert(r.x+r.width<=other.x || other.x+other.width<=r.x || r.y+r.height<=other.y || other.y+other.height<=r.y);
            }
        }
        assert(total==result.IsValid() && glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (auto issue : {FarmContestEntryIssue::Eligible, FarmContestEntryIssue::InvalidDay,
        FarmContestEntryIssue::SeasonEnded, FarmContestEntryIssue::NoReservation, FarmContestEntryIssue::InvalidRecord,
        FarmContestEntryIssue::UnknownHarvestDay, FarmContestEntryIssue::FutureHarvestDay, FarmContestEntryIssue::OutsidePeriod}) {
        FarmContestEntryResult result; result.issue=issue; result.currentDay=2147483647;
        result.contestDay=20; result.firstHarvestDay=11; result.daysRemaining=9;
        result.harvestedDay=2147483647; result.crop=farm::CropType::Carrot;
        View view; BuildContestEntryView(view,result);
        assert(view.Hit({800,150}).action==Action::ContestPreview && view.Hit({800,150}).argument==0);
        assert(view.Hit({800,380}).action==Action::HarvestInventory);
        std::size_t glyphs=0;
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
        assert(glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (auto status : {FarmContestSubmissionStatus::Ready, FarmContestSubmissionStatus::NotContestDay,
        FarmContestSubmissionStatus::AlreadySubmitted, FarmContestSubmissionStatus::Ineligible}) {
        View view; FarmContestEntryResult entry;
        BuildContestEntryView(view,entry,status,{Action::SubmitContest,7,9});
        const auto hit=view.Hit({400,532});
        assert(hit.action==(status==FarmContestSubmissionStatus::Ready ? Action::SubmitContest : Action::None));
        if(status==FarmContestSubmissionStatus::Ready) assert(hit.argument==7 && hit.inventoryGeneration==9);
        assert(view.Hit({800,532}).action==Action::ContestResults);
    }
    for (int day : {0,1,10,11,20,21,30,31,2147483647}) for (bool populated : {false,true}) {
        FarmEconomySystem::ContestResults results{};
        if(populated) for(std::size_t i=0; i<results.size(); ++i) {
            auto& result=results[i]; result.contestDay=10+static_cast<int>(i)*10;
            result.harvest.quality.crop=farm::CropType::Carrot;
            result.harvest.harvestedDay=result.contestDay;
            result.harvest.quality.harvestSize={8,true}; result.qualityPoints=60; result.sizePoints=40;
        }
        const auto summary=FarmContestSeasonSystem::Evaluate(day,results);
        View view; BuildContestResultsView(view,results,summary);
        assert(view.items[0].label==(summary.finalized ? Label::ContestSeasonFinal : Label::ContestResults));
        assert(view.Hit({900,150}).action==Action::ContestPreview);
        if (!populated && day>0) {
            for(std::size_t i=0; i<3; ++i)
                assert(view.items[3+i].label==ContestEventLabel(summary.events[i]));
        }
        std::size_t glyphs=0;
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& r=item.rect;
            assert(r.x>=150 && r.x+r.width<=1130 && r.y>=120 && r.y+r.height<=616);
            const float space=item.value.empty() ? r.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width>=20.f/26.f);
            if(!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24<=r.width);
            glyphs+=item.value.size();
            for(std::size_t j=i+1;j<view.count;++j) assert(separated(r,view.items[j].rect));
        }
        assert(glyphs<=160 && view.count+7<=View::kCapacity);
    }
    for (int day : {-1, 0, 31, 2147483647}) {
        FarmContestEntryResult result; result.currentDay=day;
        result.issue=day<1 ? FarmContestEntryIssue::InvalidDay : FarmContestEntryIssue::SeasonEnded;
        View view; BuildContestEntryView(view,result);
        assert(view.items[3].value=="--" && view.items[4].value=="--" && view.items[5].value=="--");
    }
    for (int count : {0, 1, 2, 3, 128}) for (bool unknown : {false, true}) for(int day : {0,1,10,20,30,2147483647}) {
        HarvestInventoryViewState state;
        state.records = count; state.capacity = 128; state.rowCount = (std::min)(count, state.kRows);
        state.pages = (std::max)(1, (count + state.kRows - 1) / state.kRows); state.unknownCount = unknown ? 2147483647 : 0;
        state.inventoryGeneration = 9; state.canChangeProtection = true;
        int nextId = 101;
        for (auto& row : state.rows) {
            row.id = nextId++;
            row.quality.crop = farm::CropType::Carrot; row.quality.harvestSize = {2, true};
            row.quality.score = 100; row.quality.salePrice = 2147483647; row.quantity = 2147483647;
            row.harvestedDay = day;
        }
        View view; BuildHarvestInventoryView(view, state);
        assert(view.Hit({500,550}).action==Action::ContestPreview);
        for (int i=0; i<state.rowCount; ++i) {
            const auto request = view.Hit({250.f, 300.f+i*88.f});
            assert(request.action == Action::ProtectHarvest && request.argument == 101+i && request.inventoryGeneration==9);
        }
        assert(view.count < View::kCapacity);
        assert(view.Hit({200,450}).action == Action::None);
        assert(view.Hit({800,450}).action == (state.pages > 1 ? Action::HarvestPage : Action::None));
        std::size_t glyphs = 0;
        int dateLabels = 0;
        for (std::size_t i=0; i<view.count; ++i) {
            const auto& item = view.items[i]; const auto& rect = item.rect;
            if(item.label == Label::HarvestDay || item.label == Label::HarvestDayUnknown) {
                ++dateLabels;
                assert(day > 0 ? item.value == std::to_string(day) : item.label == Label::HarvestDayUnknown);
                assert(item.request.action == Action::None);
            }
            assert(rect.x >= 150 && rect.x+rect.width <= 1130 && rect.y >= 120 && rect.y+rect.height <= 616);
            const float space = item.value.empty() ? rect.width-20 : item.valueOffset-30;
            assert(space/kLabels[static_cast<std::size_t>(item.label)].width >= 20.f/26.f);
            if (!item.value.empty()) assert(item.valueOffset + 14*(item.value.size()-1) + 24 <= rect.width);
            glyphs += item.value.size();
            for (std::size_t j=i+1; j<view.count; ++j) {
                const auto& other = view.items[j].rect;
                assert(rect.x+rect.width <= other.x || other.x+other.width <= rect.x ||
                    rect.y+rect.height <= other.y || other.y+other.height <= rect.y);
            }
        }
        assert(glyphs <= 160);
        assert(dateLabels == state.rowCount);
    }
    {
        HarvestInventoryViewState state; state.records=state.rowCount=1; state.rows[0].id=42;
        state.rows[0].saleProtected=true; state.inventoryGeneration=7;
        for (bool enabled : {false,true}) {
            state.canChangeProtection=enabled;
            View view; BuildHarvestInventoryView(view,state);
            const auto request=view.Hit({250,300});
            assert(request.action==(enabled ? Action::UnprotectHarvest : Action::None));
            if(enabled) assert(request.argument==42 && request.inventoryGeneration==7);
        }
    }
    for (bool reserved : {false,true}) for (bool eligible : {false,true}) for (bool enabled : {false,true}) {
        HarvestInventoryViewState state; state.records=state.rowCount=1; state.rows[0].id=42;
        state.rows[0].saleProtected=true; state.rows[0].canReserve=eligible; state.inventoryGeneration=77;
        state.contestReservationId=reserved ? 42 : 0; state.canChangeProtection=enabled;
        View view; BuildHarvestInventoryView(view,state);
        const auto request=view.Hit({700,300});
        const auto expected=!enabled || (!reserved && !eligible) ? Action::None :
            reserved ? Action::CancelContestReservation : Action::ReserveContestHarvest;
        assert(request.action==expected);
        if(expected!=Action::None) assert(request.argument==42 && request.inventoryGeneration==77);
        if(reserved) assert(view.Hit({250,300}).action==Action::None);
    }
    assert(CropSizeText({}) == "--");
    {
        HarvestInventoryViewState state; state.records=128; state.pages=64; state.page=63;
        state.capacity=128; state.rowCount=2; state.inventoryGeneration=99; state.canChangeProtection=true;
        state.contestReservationId=1; state.reservedCrop=farm::CropType::Carrot;
        for(int i=0; i<2; ++i) { state.rows[i].id=127+i; state.rows[i].canReserve=true; }
        View view; BuildHarvestInventoryView(view,state);
        assert(view.Hit({800,450}).action==Action::None);
        assert(view.Hit({200,450}).action==Action::HarvestPage);
        assert(view.Hit({700,300}).argument==127 && view.Hit({700,388}).argument==128);
        bool label=false;
        for(std::size_t i=0;i<view.count;++i) label |= view.items[i].label==Label::ContestCarrot;
        assert(label);
    }
    assert(CropSizeText({1.25f, true}) == "1.25x");
    assert(CropSizeText({std::numeric_limits<float>::quiet_NaN(), true}) == "--");
    for (bool harvested : {false, true}) {
        FarmCropQualityResult quality;
        quality.crop = farm::CropType::TestCrop; quality.basePrice = quality.salePrice = 120;
        quality.harvestSize = {2.f, true};
        View view; BuildQualityView(view, quality, FarmCropQualitySystem::Analyze(quality), 0, harvested);
        bool found = false;
        for (std::size_t i = 0; i < view.count; ++i) {
            if (view.items[i].label == (harvested ? Label::SizeRecorded : Label::SizeForecast)) {
                found = true; assert(view.items[i].value == "2.00x");
                const auto& bottom = QualityRadar::axisLabels[2];
                assert(bottom.y + bottom.height + 8 <= view.items[i].rect.y);
            }
        }
        assert(found);
        for (const auto& label : QualityRadar::axisLabels) {
            assert(label.width == 24 && label.height == 36);
            for (std::size_t i = 0; i < view.count; ++i) {
                const auto& item = view.items[i].rect;
                assert(label.x + label.width <= item.x || item.x + item.width <= label.x ||
                    label.y + label.height <= item.y || item.y + item.height <= label.y);
            }
        }
    }
    for (bool preview : {false,true}) for (bool allowed : {false,true}) for (bool intake : {false,true})
    for (const auto issue : {farm::FarmCanalPathIssue::None, farm::FarmCanalPathIssue::NonStraight,
        farm::FarmCanalPathIssue::BlockedTile}) for (int blocked : {-1, 639}) {
        TerrainViewState state;
        state.preview = preview; state.canConfirm = allowed;
        state.canRaise = state.canLower = state.canCanal = state.canSource = allowed;
        state.canPath = state.canUndo = state.canRedo = state.canCompost = allowed;
        state.canSetIrrigation = allowed; state.irrigationEnabled = intake;
        state.tileValues = "639 / H3 / 100 / 100";
        state.changeCount = preview ? 640 : 0;
        state.pathIssue = issue;
        state.blockedTileIndex = blocked;
        View terrain;
        BuildTerrainView(terrain, state);
        assert(terrain.terrain && !terrain.modal && terrain.count == 18);
        assert(terrain.items[1].selected == (allowed && intake));
        assert(terrain.items[2].selected == (allowed && !intake));
        assert(terrain.Hit({700,40}).action == (!preview && allowed ? Action::SetIrrigation : Action::None));
        assert(terrain.Hit({830,40}).action == (!preview && allowed ? Action::SetIrrigation : Action::None));
        if (!preview && allowed) {
            assert(terrain.Hit({700,40}).argument == 1 && terrain.Hit({830,40}).argument == 0);
        }
        const bool rejected = preview && issue != farm::FarmCanalPathIssue::None;
        const Label expectedStatus = preview && !allowed ? Label::PreviewStale :
            !preview || issue == farm::FarmCanalPathIssue::None ? state.status :
            issue == farm::FarmCanalPathIssue::BlockedTile ? Label::PathBlocked : Label::PathNonStraight;
        assert(terrain.items[0].label == expectedStatus);
        if (expectedStatus == Label::PathBlocked)
            assert(terrain.items[0].value == (blocked < 0 ? "--" : "#639"));
        else assert(terrain.items[0].value.empty());
        assert(terrain.items[14].label == (rejected && allowed ? Label::ConfirmCandidates : Label::Confirm));
        assert(terrain.items[5].value == (preview ? "640" : "0"));
        assert(terrain.Hit({50,550}).action == (!preview && allowed ? Action::Raise : Action::None));
        assert(terrain.Hit({50,660}).action == (preview && allowed ? Action::Confirm : Action::None));
        assert(terrain.Hit({350,660}).action == (preview ? Action::Cancel : Action::None));
        assert(terrain.Hit({1050,40}).action == Action::TerrainExit);
        assert(terrain.Covers({20,530}) && terrain.Covers({335,550}) && terrain.Covers({20,108}));
        assert(!terrain.Covers({640,300}) && !terrain.Covers({640,519}));
        assert(terrain.Hit({335,550}).action == Action::None);
        assert(!terrain.Covers({1264,550}) && !terrain.Covers({640,704}));
        for (std::size_t i=0; i<terrain.count; ++i) {
            const auto& item = terrain.items[i];
            assert(item.rect.x >= 16 && item.rect.x+item.rect.width <= 1264);
            assert(item.rect.y >= 16 && item.rect.y+item.rect.height <= 704);
            const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
            const float space = item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/uv.width >= 20.0f/26.0f);
            if (!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24 <= item.rect.width);
            for (std::size_t j=i+1; j<terrain.count; ++j) {
                const auto& other = terrain.items[j].rect;
                assert(item.rect.x+item.rect.width <= other.x || other.x+other.width <= item.rect.x ||
                    item.rect.y+item.rect.height <= other.y || other.y+other.height <= item.rect.y);
            }
        }
        View quick;
        BuildPlayQuickView(quick, preview, allowed);
        assert(quick.Hit({280,40}).action == (allowed ? Action::TerrainField : Action::None));
        assert(quick.Hit({350,40}).action == Action::Pause);
        assert(quick.items[1].label == (preview ? Label::Play : Label::Pause));
        for (std::size_t i=0; i<quick.count; ++i) {
            const auto& rect = quick.items[i].rect;
            assert(!View::FarmHUDCovers({rect.x,rect.y}));
            assert(!View::FarmHUDCovers({rect.x+rect.width-1,rect.y+rect.height-1}));
        }
    }
    assert(kAsciiY + 128 <= 4096);
    for (bool paused : {false,true}) for (bool canEdit : {false,true})
    for (bool canControlTime : {false,true})
    for (float speed : {1.0f,2.0f,4.0f,3.0f,std::numeric_limits<float>::quiet_NaN()}) {
        View quick;
        BuildPlayQuickView(quick, paused, canEdit, canControlTime, speed);
        assert(quick.count == 5);
        assert(quick.Hit({350,40}).action == (canControlTime ? Action::Pause : Action::None));
        assert(quick.Hit({280,40}).action == (canEdit ? Action::TerrainField : Action::None));
        assert(quick.items[0].icon == HUDIcon::Terrain);
        assert(quick.items[1].icon == (paused ? HUDIcon::Play : HUDIcon::Pause));
        int selected = 0;
        for (std::size_t i = 0; i < quick.count; ++i) {
            const auto& item = quick.items[i];
            AssertReadable(item);
            AssertHalfOpenControl(item);
            for (const auto& panel : View::kFarmHudPanels) assert(separated(item.rect, panel));
            for (std::size_t j=i+1; j<quick.count; ++j) assert(separated(item.rect,quick.items[j].rect));
            if (i < 2) continue;
            const int expected = i == 2 ? 1 : i == 3 ? 2 : 4;
            assert(item.request.argument == expected);
            const Vector2 center{item.rect.x+item.rect.width/2,item.rect.y+item.rect.height/2};
            const auto hit = quick.Hit(center);
            assert(hit.action == (canControlTime ? Action::Speed : Action::None));
            if (canControlTime) assert(hit.argument == expected);
            assert(quick.Covers(center));
            assert(item.selected == (canControlTime && !paused && speed == expected));
            selected += item.selected ? 1 : 0;
        }
        assert(selected == (canControlTime && !paused && (speed==1 || speed==2 || speed==4) ? 1 : 0));
        assert(!quick.Covers({650,40}) && quick.Hit({650,40}).action == Action::None);
    }
    const Rect farmSafeArea{farm::kFarmOverviewFrame.left*1280, farm::kFarmOverviewFrame.top*720,
        (farm::kFarmOverviewFrame.right-farm::kFarmOverviewFrame.left)*1280,
        (farm::kFarmOverviewFrame.bottom-farm::kFarmOverviewFrame.top)*720};
    assert(separated(farmSafeArea, View::kWaterGuidancePanel));
    assert(farmSafeArea.x >= View::kWaterGuidancePanel.x+View::kWaterGuidancePanel.width+8);
    for (bool visible : {false,true}) for (bool closed : {false,true}) for (bool editable : {false,true})
    for (bool canSetIntake : {false,true})
    for (auto supply : {farm::FarmWaterStatus::None, farm::FarmWaterStatus::Available,
        farm::FarmWaterStatus::Retained, farm::FarmWaterStatus::Waiting, farm::FarmWaterStatus::Dry})
    for (auto advice : {FarmWaterAdvice::Unknown, FarmWaterAdvice::Till, FarmWaterAdvice::Plant,
        FarmWaterAdvice::Harvest, FarmWaterAdvice::Water, FarmWaterAdvice::CheckSupply,
        FarmWaterAdvice::CloseIntake, FarmWaterAdvice::AvoidWater, FarmWaterAdvice::Monitor}) {
        View status; BuildWaterGuidanceView(status, {visible,closed,supply,advice}, editable, canSetIntake);
        assert(status.count == (visible ? 3u : 0u));
        assert(status.waterGuidance == visible);
        assert(status.Hit({100,470}).action == (visible && editable ? Action::TerrainField : Action::None));
        assert(status.Hit({100,510}).action == Action::None);
        assert(status.Covers({100,490}) == visible);
        const auto intake = status.Hit({300,470});
        assert(intake.action == (visible && editable && canSetIntake ? Action::SetIrrigation : Action::None));
        if (intake.action == Action::SetIrrigation) assert(intake.argument == (closed ? 1 : 0));
        assert(status.Covers({300,470}) == visible);
        assert(status.Hit({280,470}).action == Action::None);
        assert(!status.Covers({500,490}) && !status.Covers({940,510}));
        if (visible) {
            assert(status.items[1].warning == (advice == FarmWaterAdvice::CloseIntake || advice == FarmWaterAdvice::AvoidWater));
            assert(status.items[2].label == (closed ? Label::IntakeOn : Label::IntakeOff));
            assert(status.items[2].icon == HUDIcon::Canal);
            AssertItemsDoNotOverlap(status);
        }
        if (visible && closed) assert(status.items[0].label == Label::IntakeClosedShort);
        for (std::size_t i=0; i<status.count; ++i) {
            const auto& item = status.items[i];
            assert(!View::FarmHUDCovers({item.rect.x,item.rect.y}));
            assert(!View::FarmHUDCovers({item.rect.x+item.rect.width-1,item.rect.y+item.rect.height-1}));
            AssertReadable(item);
            AssertHalfOpenControl(item);
        }
        if (visible) assert(status.items[0].rect.y+status.items[0].rect.height <= status.items[1].rect.y);
    }
    for (const Label label : {Label::RaiseBrush, Label::LowerBrush}) {
        TerrainViewState state;
        state.preview = state.canConfirm = true; state.status = label;
        View heightView; BuildTerrainView(heightView, state);
        assert(heightView.items[0].label == label && heightView.count == 18);
        assert((heightView.items[0].rect.width - 20) / kLabels[static_cast<std::size_t>(label)].width >= 20.0f/26.0f);
    }
    for (bool harvested : {false,true}) {
        View empty;
        BuildQualityView(empty, {}, {}, -1, harvested);
        assert(!empty.radar.visible && empty.count == 3);
        assert(empty.Hit({700,150}).action == Action::Quality);
        FarmCropQualityResult quality;
        quality.crop = farm::CropType::Carrot; quality.basePrice = 170; quality.salePrice = 210;
        quality.score = 85; quality.maturity = 0.75f; quality.waterBalance = 1;
        quality.terrainFit = 0.5f; quality.nutrientBalance = 0.25f; quality.nutrientKnown = true;
        View chart;
        BuildQualityView(chart, quality, FarmCropQualitySystem::Analyze(quality), 12, harvested);
        assert(chart.items[chart.count-1].label == Label::HintNutrients);
        assert(chart.radar.visible && chart.radar.known[3] && chart.radar.values[3] == 0.25f);
        assert(chart.count + 7 < View::kCapacity);
        assert(chart.items[0].selected != harvested && chart.items[1].selected == harvested);
        for (std::size_t i=0; i<chart.count; ++i) {
            const auto& item = chart.items[i];
            assert(item.rect.x >= 150 && item.rect.x+item.rect.width <= 1130);
            assert(item.rect.y >= 130 && item.rect.y+item.rect.height <= 618);
            const auto& uv = kLabels[static_cast<std::size_t>(item.label)];
            const float space = item.value.empty() ? item.rect.width-20 : item.valueOffset-30;
            assert(space/uv.width >= 20.0f/26.0f);
            if (!item.value.empty()) assert(item.valueOffset+14*(item.value.size()-1)+24 <= item.rect.width);
            for (std::size_t j=i+1; j<chart.count; ++j) {
                const auto& other = chart.items[j].rect;
                assert(item.rect.x+item.rect.width <= other.x || other.x+other.width <= item.rect.x ||
                    item.rect.y+item.rect.height <= other.y || other.y+other.height <= item.rect.y);
            }
        }
        quality.nutrientKnown = false;
        quality.waterBalance = std::numeric_limits<float>::quiet_NaN();
        quality.terrainFit = 3;
        View old;
        BuildQualityView(old, quality, FarmCropQualitySystem::Analyze(quality), -1, harvested);
        assert(old.items[old.count-3].label == (harvested ? Label::HintPartialRecord : Label::HintPartialEstimate));
        assert(!old.radar.known[3] && !old.radar.known[1] && old.radar.values[2] == 1);
    }
    assert(QualityRadar::Point(0,1).y == QualityRadar::center.y-QualityRadar::radius);
    assert(QualityRadar::Point(1,2).x == QualityRadar::center.x+QualityRadar::radius);
    assert(QualityRadar::Point(3,-1).x == QualityRadar::center.x);
    assert(QualityRadar::Point(99,1).x == QualityRadar::center.x);
    assert(QualityRadar::Point(0,std::numeric_limits<float>::infinity()).y == QualityRadar::center.y);
    for (auto focus : {FarmQualityFocus::Maturity, FarmQualityFocus::Water, FarmQualityFocus::Terrain,
        FarmQualityFocus::Nutrients, FarmQualityFocus::Balanced, FarmQualityFocus::Unknown}) {
        for (bool harvested : {false,true}) {
            const auto& uv = kLabels[static_cast<std::size_t>(QualityHintLabel(focus,harvested))];
            assert(uv.width <= 900 && uv.height <= 38);
        }
    }
    assert(View::FarmHUDCovers({24,24}) && View::FarmHUDCovers({239.99f,79.99f}));
    assert(!View::FarmHUDCovers({240,24}) && !View::FarmHUDCovers({24,80}));
    assert(View::FarmHUDCovers({1000,24}) && View::FarmHUDCovers({1255.99f,79.99f}));
    assert(View::FarmHUDCovers({24,554}) && View::FarmHUDCovers({390,648}));
    assert(!View::FarmHUDCovers({876,24}) && !View::FarmHUDCovers({24,520}));
    assert(!View::FarmHUDCovers({640,300}) && !View::FarmHUDCovers({1279,719}));
    for (const auto frame : {farm::kFarmOverviewFrame, farm::kObservationOverviewFrame, farm::kTerrainOverviewFrame}) {
        for (float aspect : {4.0f/3, 16.0f/9, 21.0f/9}) {
            for (float extent : {0.6f, 3.5f, 12.0f}) {
                for (float height : {0.0f, 1.16f, 4.0f}) {
                    const Vector3 minimum{-extent, 0.05f, 8-extent};
                    const Vector3 maximum{extent, 0.05f+height, 8+extent};
                    const float py = 1/std::tan(0.45f/2), px = py/aspect;
                    farm::OverviewPose pose;
                    assert(farm::FitOverviewCamera(minimum, maximum, px, py, 0.1f, 1000, frame, pose));
                    for (int corner = 0; corner < 8; ++corner) {
                        const float dx = ((corner&1) ? maximum.x : minimum.x)-pose.position.x;
                        const float dy = ((corner&2) ? maximum.y : minimum.y)-pose.position.y;
                        const float dz = ((corner&4) ? maximum.z : minimum.z)-pose.position.z;
                        const float cameraY = std::cos(pose.rotation.x)*dy+std::sin(pose.rotation.x)*dz;
                        const float cameraZ = -std::sin(pose.rotation.x)*dy+std::cos(pose.rotation.x)*dz;
                        const float screenX = (px*dx/cameraZ+1)*0.5f;
                        const float screenY = (1-py*cameraY/cameraZ)*0.5f;
                        assert(cameraZ > 0.1f && cameraZ < 1000);
                        assert(screenX >= frame.left-0.0001f && screenX <= frame.right+0.0001f);
                        assert(screenY >= frame.top-0.0001f && screenY <= frame.bottom+0.0001f);
                    }
                }
            }
        }
    }
    for (const auto frame : {farm::kFarmOverviewFrame, farm::kObservationOverviewFrame, farm::kTerrainOverviewFrame}) {
        for (float aspect : {4.f/3, 16.f/9, 21.f/9}) for (float x : {-8.f, 0.f, 8.f})
        for (float z : {-4.f, 8.f, 18.f}) for (float height : {0.f, 3.f}) {
            const Vector3 farmMin{-3,0,5}, farmMax{3,2,11};
            const Vector3 playerCenter{x,height+.9f,z}, playerHalf{.45f,.9f,.45f};
            const float py = 1/std::tan(.45f/2), px = py/aspect;
            farm::OverviewPose pose;
            assert(farm::FitFarmFollowCamera(farmMin, farmMax, playerCenter, playerHalf,
                px, py, .1f, 1000, frame, pose));
            const std::array minima{farmMin, Vector3{x-.70f,height-.25f,z-.70f}};
            const std::array maxima{farmMax, Vector3{x+.70f,height+2.05f,z+.70f}};
            for (std::size_t box=0; box<minima.size(); ++box) for (int corner=0; corner<8; ++corner) {
                const auto& low=minima[box]; const auto& high=maxima[box];
                const float dx=((corner&1)?high.x:low.x)-pose.position.x;
                const float dy=((corner&2)?high.y:low.y)-pose.position.y;
                const float dz=((corner&4)?high.z:low.z)-pose.position.z;
                const float cameraY=std::cos(pose.rotation.x)*dy+std::sin(pose.rotation.x)*dz;
                const float cameraZ=-std::sin(pose.rotation.x)*dy+std::cos(pose.rotation.x)*dz;
                const float screenX=(px*dx/cameraZ+1)*.5f, screenY=(1-py*cameraY/cameraZ)*.5f;
                assert(cameraZ>.1f && cameraZ<1000);
                assert(screenX>=frame.left-.0001f && screenX<=frame.right+.0001f);
                assert(screenY>=frame.top-.0001f && screenY<=frame.bottom+.0001f);
            }
        }
    }
    farm::OverviewPose unchanged{{123,456,789}, {0,0,0}};
    for (float invalid : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{0,0,0},{invalid,1,1},2,4,.1f,100,
            farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({2,0,0},{1,1,1},{0,0,0},{1,1,1},2,4,.1f,100,farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{std::numeric_limits<float>::quiet_NaN(),0,0},
        {1,1,1},2,4,.1f,100,farm::kFarmOverviewFrame,unchanged));
    assert(!farm::FitFarmFollowCamera({0,0,0},{1,1,1},{0,0,0},{1,1,1},2,4,.1f,1,farm::kFarmOverviewFrame,unchanged));
    std::cout << "PASS: follow camera farm/player safe bounds, 162 poses, invalid input retains pose\n";
    for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, invalid, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({2,0,0}, {1,1,1}, 2, 4, 0.1f, 100, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 1, farm::kFarmOverviewFrame, unchanged));
    assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, 0.1f, 100, {0,0,0,1}, unchanged));
    for (float pitch : {0.f, 1.4f, std::numeric_limits<float>::quiet_NaN()})
        assert(!farm::FitOverviewCamera({0,0,0}, {1,1,1}, 2, 4, .1f, 100,
            farm::kFarmOverviewFrame, unchanged, pitch));
    assert(unchanged.position.x == 123 && unchanged.position.y == 456 && unchanged.position.z == 789);
    for (bool follow : {false,true}) for (bool available : {false,true}) {
        View camera;
        BuildCameraView(camera, follow, available);
        assert(camera.count == (available ? 2u : 1u));
        assert(camera.Hit(Center(HUDLayout::UtilitySlot(2))).action == Action::Overview);
        assert(camera.Hit(Center(HUDLayout::UtilitySlot(3))).action == (available ? Action::Follow : Action::None));
        assert(camera.Covers(Center(HUDLayout::UtilitySlot(3))) == available);
        assert(!camera.Covers({1100,450}) && !camera.Covers({1100,500}));
        assert(camera.items[0].selected == !follow && camera.items[0].icon == HUDIcon::Overview);
        if (available) assert(camera.items[1].selected == follow && camera.items[1].icon == HUDIcon::Follow);
        AssertItemsDoNotOverlap(camera);
        for (std::size_t i=0; i<camera.count; ++i) {
            const auto& item = camera.items[i];
            AssertReadable(item);
            AssertHalfOpenControl(item);
            assert(item.rect.y+item.rect.height < View::kFieldActionsPanel.y);
        }
    }
    View view;
    for (int target : {-1, 0, 19, 639}) {
        FarmToolActionResult evaluation;
        View ended;
        BuildFieldActionView(ended, evaluation, false, true, FarmHUDNextAction::SelectTile, true);
        assert(ended.count == 2 && ended.items[0].label == Label::SeasonEnded && !ended.items[1].enabled);
        View stopped;
        BuildPlayQuickView(stopped, false, false, false);
        assert(stopped.items[1].label == Label::Paused && stopped.Hit({350,40}).action == Action::None);
        evaluation.tileIndex = target;
        evaluation.status = target < 0 ? FarmToolActionStatus::InvalidTarget : FarmToolActionStatus::Applied;
        View targetView;
        BuildFieldActionView(targetView, evaluation, false, false);
        assert(targetView.count == 2);
        const auto& reason = targetView.items[0];
        assert(reason.label == (target < 0 ? Label::ActionNoTile : Label::ActionReady));
        assert(reason.request.action == Action::None && reason.value.empty());
        assert(targetView.Covers(Center(reason.rect)) && targetView.Hit(Center(reason.rect)).action == Action::None);
        assert(!targetView.Covers({1100,550}));
        AssertReadable(reason);
        evaluation.status = FarmToolActionStatus::InvalidTarget;
        View invalid;
        BuildFieldActionView(invalid, evaluation, false, false);
        assert(invalid.items[0].label == Label::ActionNoTile && invalid.items[0].value.empty());
    }
    for (const auto next : {FarmHUDNextAction::Hoe, FarmHUDNextAction::WaterOrSeed,
        FarmHUDNextAction::Seed, FarmHUDNextAction::Water, FarmHUDNextAction::Harvest, FarmHUDNextAction::BuySeed,
        FarmHUDNextAction::Growing, FarmHUDNextAction::ReduceWater, FarmHUDNextAction::Canal,
        FarmHUDNextAction::WaterSource, FarmHUDNextAction::SelectTile}) {
        for (int blocked = 0; blocked < 4; ++blocked) {
            View field;
            FarmToolActionResult evaluation;
            evaluation.status = FarmToolActionStatus::InvalidState;
            evaluation.tool = next == FarmHUDNextAction::Hoe ? FarmTool::Seed : FarmTool::Hoe;
            BuildFieldActionView(field, evaluation, (blocked & 1) != 0, (blocked & 2) != 0, next);
            const auto hit = field.Hit({1100, 665});
            const int expectedTool = next == FarmHUDNextAction::Hoe ? 0 :
                (next == FarmHUDNextAction::Water || next == FarmHUDNextAction::WaterOrSeed) ? 1 :
                next == FarmHUDNextAction::Seed ? 2 : next == FarmHUDNextAction::Harvest ? 3 : -1;
            if (blocked & 2) assert(hit.action == Action::None);
            else if (expectedTool >= 0) { assert(hit.action == Action::Tool && hit.argument == expectedTool); }
            else assert(hit.action == (next == FarmHUDNextAction::BuySeed ? Action::OpenSeedShop : Action::None));
            assert(hit.action != Action::ApplyTool); // A next-tool shortcut never farms implicitly.
            assert(field.Covers({1100, 665}));
            for (std::size_t i = 0; i < field.count; ++i) {
                const auto& item = field.items[i];
                AssertReadable(item);
                AssertHalfOpenControl(item);
            }
        }
    }
    View sameTool;
    FarmToolActionResult unavailable;
    unavailable.status = FarmToolActionStatus::InvalidState;
    unavailable.tool = FarmTool::Hoe;
    BuildFieldActionView(sameTool, unavailable, false, false, FarmHUDNextAction::Hoe);
    assert(sameTool.Hit({1100, 665}).action == Action::None);
    for (const auto status : {FarmToolActionStatus::None, FarmToolActionStatus::Applied,
        FarmToolActionStatus::Harvested, FarmToolActionStatus::InvalidTarget, FarmToolActionStatus::InvalidState,
        FarmToolActionStatus::AlreadyWatered, FarmToolActionStatus::NoSeed, FarmToolActionStatus::NotReady,
        FarmToolActionStatus::UnsupportedTool, FarmToolActionStatus::InventoryFull}) {
        for (const auto tool : {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest}) {
            for (int blocked = 0; blocked < 4; ++blocked) {
                View field;
                FarmToolActionResult evaluation;
                evaluation.status = status; evaluation.tool = tool;
                BuildFieldActionView(field, evaluation, (blocked & 1) != 0, (blocked & 2) != 0);
                field.IconButton(Label::Menu, HUDIcon::Menu, HUDLayout::UtilitySlot(5), {Action::Menu});
                const auto expected = (blocked & 2) != 0 ? Action::None : status == FarmToolActionStatus::NoSeed
                    ? Action::OpenSeedShop : evaluation.Succeeded() ? Action::ApplyTool : Action::None;
                assert(field.Hit({1100, 665}).action == expected);
                assert(field.Covers({1100, 665}) && field.Covers({958, 586}));
                assert(field.Hit({1100, 610}).action == Action::None);
                assert(field.Hit(Center(HUDLayout::UtilitySlot(5))).action == Action::Menu);
                assert(!field.Covers({640, 300}));
                AssertItemsDoNotOverlap(field);
                for (std::size_t i = 0; i < field.count; ++i) {
                    const auto& a = field.items[i].rect;
                    assert(a.x >= 956 && a.x+a.width <= 1256 && a.y >= 584 && a.y+a.height <= 708);
                    AssertReadable(field.items[i]);
                    AssertHalfOpenControl(field.items[i]);
                    for (std::size_t j = i+1; j < field.count; ++j) {
                        const auto& b = field.items[j].rect;
                        assert(a.x+a.width <= b.x || b.x+b.width <= a.x || a.y+a.height <= b.y || b.y+b.height <= a.y);
                    }
                }
            }
        }
    }
    for (int mode : {0,1,2}) for (bool skip : {false,true}) {
        FarmHUDViewData data; data.day=2147483647; data.goalMoney=2147483647;
        data.contestSeason=mode==1; data.freeFarming=mode==2;
        FarmStartPresentationSystem::Frame frame;
        frame.canSkip=skip; frame.showGoal=true;
        View intro; BuildFarmStartView(intro,frame,data);
        assert(intro.starting && intro.modal && intro.Covers({640,360}));
        assert((intro.Hit({1100,675}).action==Action::StartSkip)==skip);
        for(std::size_t i=0;i<intro.count;++i) {
            const auto& a=intro.items[i];
            const float width=a.value.empty()?a.rect.width-20:a.valueOffset-30;
            assert(width/kLabels[static_cast<std::size_t>(a.label)].width>=20.f/26.f);
            if(!a.value.empty()) assert(a.valueOffset+14*(a.value.size()-1)+24<=a.rect.width);
            for(std::size_t j=i+1;j<intro.count;++j) assert(separated(a.rect,intro.items[j].rect));
        }
    }
    for (const auto phase : {FarmPlayFlow::Phase::Briefing, FarmPlayFlow::Phase::Result}) {
        View entry;
        FarmHUDViewData data;
        data.money = 300; data.goalMoney = 540;
        BuildPlayFlowView(entry, phase, data);
        assert(entry.modal && entry.Covers({640, 300}));
        if (phase == FarmPlayFlow::Phase::Briefing) {
            assert(entry.briefing);
            assert(entry.Hit({200, 590}).action == Action::FlowContinue);
            assert(entry.Hit({700, 660}).action == Action::Restart);
            assert(entry.Hit({700, 590}).action == Action::FlowRecords);
            assert(entry.Hit({600, 300}).action == Action::None);
        } else {
            assert(entry.Hit({200, 510}).action == Action::FlowContinue);
            assert(entry.Hit({700, 510}).action == Action::Restart);
            assert(entry.Hit({200, 570}).action == Action::FlowRecords);
        }
        for (std::size_t i = 0; i < entry.count; ++i) {
            const auto& a = entry.items[i].rect;
            assert(a.x >= 0 && a.x+a.width <= 1280 && a.y >= 0 && a.y+a.height <= 688);
            for (std::size_t j = i+1; j < entry.count; ++j) {
                const auto& b = entry.items[j].rect;
                assert(a.x+a.width <= b.x || b.x+b.width <= a.x || a.y+a.height <= b.y || b.y+b.height <= a.y);
            }
        }
    }
    assert(view.Hit({0, 0}).action == Action::None);
    assert(view.NextActionable(-1, 1) == -1);
    view.Add(Label::Menu, {10, 20, 100, 40}, {Action::Menu});
    view.Add(Label::Save, {120, 20, 100, 40}, {Action::Save}, false);
    view.Add(Label::Ready, {10, 80, 100, 40});
    view.Add(Label::Cancel, {120, 80, 100, 40}, {Action::Cancel});
    assert(view.Hit({10, 20}).action == Action::Menu);
    assert(view.Hit({110, 20}).action == Action::None);
    assert(view.Hit({10, 60}).action == Action::None);
    assert(view.Hit({130, 30}).action == Action::None);
    assert(view.Covers({130, 30})); // Disabled buttons must not click through.
    assert(view.Covers({20, 90})); // Noninteractive labels also own their area.
    assert(!view.Covers({500, 500}));
    assert(view.Hit({std::numeric_limits<float>::quiet_NaN(), 20}).action == Action::None);
    assert(view.Hit({std::numeric_limits<float>::infinity(), 20}).action == Action::None);
    assert(view.NextActionable(-1, 1) == 0);
    assert(view.NextActionable(0, 1) == 3);
    assert(view.NextActionable(3, 1) == 0);
    assert(view.NextActionable(0, -1) == 3);
    view.Add(Label::Accept, {10, 20, 100, 40}, {Action::Accept});
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (int i = 0; i < 100; ++i) view.Add(Label::Menu, {}, {Action::Menu});
    assert(view.count == View::kCapacity);
    view.count = 1000;
    assert(view.Covers({130, 30}));
    assert(view.Hit({20, 30}).action == Action::Accept);
    for (auto& item : view.items) item.enabled = false;
    assert(view.NextActionable(1, 1) == -1);
    assert(view.Hit({20, 30}).action == Action::None);
    for (const auto& uv : kLabels) {
        assert(std::isfinite(uv.x) && std::isfinite(uv.y));
        assert(uv.width > 0 && uv.height > 0);
        assert(uv.x >= 0 && uv.x + uv.width <= kAtlasWidth && kAtlasWidth <= 4096);
        assert(uv.y >= 0 && uv.y + uv.height <= kAsciiY);
    }
    FarmComparisonView comparison{};
    for (const auto status : {FarmComparisonStatus::Idle, FarmComparisonStatus::Running,
        FarmComparisonStatus::Stopped, FarmComparisonStatus::Completed, FarmComparisonStatus::Invalidated}) {
        for (int picking = -1; picking < 2; ++picking) {
            View observation;
            comparison.status = status;
            comparison.startIssue = FarmComparisonIssue::None;
            comparison.rows[0].tileIndex = 12;
            comparison.rows[1].tileIndex = 13;
            BuildObservationView(observation, comparison, {true, false}, picking, 12, true, 2, true);
            assert(observation.count < View::kCapacity && observation.observation && !observation.modal);
            assert(observation.Covers({30, 30}) && observation.Covers({1250, 680}));
            assert(!observation.Covers({640, 300}));
            assert(observation.Hit({400, 445}).action == Action::JumpSlot);
            assert(observation.Hit({1000, 445}).action == Action::None);
            assert((observation.Hit({60, 516}).action == Action::PickSlot) == (status != FarmComparisonStatus::Running));
            assert((observation.Hit({60, 626}).action == Action::Start) == (status != FarmComparisonStatus::Running));
            assert((observation.Hit({300, 626}).action == Action::Stop) == (status == FarmComparisonStatus::Running));
            for (std::size_t i = 0; i < observation.count; ++i) {
                const auto& r = observation.items[i].rect;
                assert(r.x >= 0 && r.y >= 0 && r.x+r.width <= 1280 && r.y+r.height <= 688);
                for (std::size_t j = i+1; j < observation.count; ++j) {
                    const auto& b = observation.items[j].rect;
                    assert(r.x+r.width <= b.x || b.x+b.width <= r.x || r.y+r.height <= b.y || b.y+b.height <= r.y);
                }
            }
        }
    }
    for (const auto issue : {FarmComparisonIssue::PickTwo, FarmComparisonIssue::SameTile, FarmComparisonIssue::NotGrowing,
        FarmComparisonIssue::Crop, FarmComparisonIssue::Height, FarmComparisonIssue::Growth, FarmComparisonIssue::InvalidData}) {
        comparison.status = FarmComparisonStatus::Idle; comparison.startIssue = issue;
        View observation;
        BuildObservationView(observation, comparison, {false, false}, -1, -1, true, 1, false);
        assert(observation.Hit({60, 626}).action == Action::None);
        assert(ComparisonIssueLabel(issue) != Label::CompareReady);
    }
    std::cout << "PASS: hit boundaries, disabled actions, navigation, capacity, atlas bounds\n";
    TestCaptionLayout();
    assert(layoutValid && "HUD layout/readability regressions reported above");
}
