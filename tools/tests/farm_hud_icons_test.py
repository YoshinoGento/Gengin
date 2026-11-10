"""CPU asset/source contracts for the compact HUD, not a GPU rendering test.

Run with Python and Pillow. Assets, saves, and submissions are read-only.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import sys
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / "project/application/farm/ui"
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools/ui"))

ICON_NAMES = (
    ("Hoe", "hoe"), ("WateringCan", "watering_can"),
    ("SeedBag", "seed_bag"), ("HarvestBasket", "harvest_basket"),
    ("Carrot", "carrot"), ("Tomato", "tomato"), ("Pumpkin", "pumpkin"),
    ("WaterDrop", "water_drop"), ("Sprout", "sprout"), ("Calendar", "calendar"),
    ("Coin", "coin"), ("Terrain", "terrain_steps"), ("Pause", "pause"),
    ("Play", "play"), ("Overview", "overview_camera"), ("Follow", "follow_person"),
    ("Menu", "menu"), ("Shop", "shop"), ("SeedCarrot", "seed_carrot"),
    ("SeedTomato", "seed_tomato"), ("SeedPumpkin", "seed_pumpkin"),
    ("Rank", "rank_rosette"), ("Canal", "canal"), ("Source", "water_source"),
    ("Back", "back"), ("Check", "check"), ("Undo", "undo"), ("Redo", "redo"),
    ("Plus", "plus"), ("Minus", "minus"), ("Close", "close"),
    ("Inventory", "inventory_crate"),
)


def source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class HUDIconTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = source(UI / "FarmRuntimeUI.h")
        cls.renderer = source(UI / "FarmRuntimeUI.cpp")
        cls.icons = source(UI / "FarmHUDIcons.h")
        cls.hud = source(UI / "FarmHUD.cpp")
        cls.controller = source(ROOT / "project/application/farm/system/FarmRuntimeController.cpp")
        cls.scene = source(ROOT / "project/application/gameplay/GamePlaySessionSystem.cpp")
        spec = importlib.util.spec_from_file_location(
            "farm_hud_icon_generator", ROOT / "tools/ui/generate_farm_hud_icons.py"
        )
        if spec is None or spec.loader is None:
            raise RuntimeError("Cannot load the icon generator")
        cls.generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.generator)

    def test_runtime_indices_match_generator_order(self):
        enum = re.search(r"enum class HUDIcon\s*:\s*int\s*\{([^}]+)\}", self.icons)
        self.assertIsNotNone(enum)
        entries = [entry.strip() for entry in enum.group(1).split(",") if entry.strip()]
        self.assertRegex(entries[0], r"^None\s*=\s*-1$")
        self.assertEqual(entries[1:], [name for name, _ in ICON_NAMES])
        self.assertEqual([name for name, _ in self.generator.ICONS], [name for _, name in ICON_NAMES])
        self.assertEqual(len(ICON_NAMES), 32)
        self.assertRegex(self.icons, r"kHUDIconSize\s*=\s*64\.0f")
        self.assertRegex(self.icons, r"kHUDIconAtlas\s*=\s*\"Resources/ui/farm_hud_icons.png\"")

    def test_atlas_matches_deterministic_generator(self):
        atlas_path = ROOT / "project/Resources/ui/farm_hud_icons.png"
        atlas = self.generator.generate_atlas()
        self.assertEqual((self.generator.CELL, self.generator.COLUMNS, self.generator.ROWS), (64, 8, 4))
        self.assertEqual(atlas_path.read_bytes(), self.generator.png_bytes(atlas), "stale icon atlas")
        with Image.open(atlas_path) as saved:
            self.assertEqual(saved.mode, "RGBA")
            self.assertEqual(saved.size, (512, 256))
            cells = set()
            for index, (name, _) in enumerate(ICON_NAMES):
                with self.subTest(index=index, icon=name):
                    x, y = index % 8 * 64, index // 8 * 64
                    cell = saved.crop((x, y, x + 64, y + 64))
                    bounds = cell.getchannel("A").getbbox()
                    self.assertIsNotNone(bounds, "empty icon cell")
                    self.assertGreaterEqual(bounds[0], 4)
                    self.assertGreaterEqual(bounds[1], 4)
                    self.assertLessEqual(bounds[2], 60)
                    self.assertLessEqual(bounds[3], 60)
                    self.assertNotIn(cell.tobytes(), cells, "duplicate icon cell")
                    cells.add(cell.tobytes())

    def test_fixed_pools_have_independent_bounded_draw_slots(self):
        self.assertRegex(self.header, r"kCapacity\s*=\s*40\s*;")
        self.assertRegex(self.header, r"std::array<Sprite,\s*84>\s+panels_")
        self.assertRegex(self.header, r"std::array<Sprite,\s*(?:View::kCapacity|40)>\s+icons_")
        for name, count in (("panels", "panelCount"), ("icons", "iconCount")):
            with self.subTest(pool=name):
                self.assertRegex(self.renderer, rf"for\s*\(auto& sprite\s*:\s*{name}_\)")
                self.assertRegex(self.renderer, rf"{count}_\s*>=\s*{name}_\.size\(\)")
                self.assertRegex(self.renderer, rf"auto& sprite\s*=\s*{name}_\[{count}_\+\+\]")
        self.assertRegex(self.renderer, r"panelCount_\s*=\s*labelCount_\s*=\s*digitCount_\s*=\s*iconCount_\s*=\s*0")
        self.assertIn("sprite.SetTextureRect(IconUV(icon)", self.renderer)

    def test_icon_branch_does_not_render_button_label(self):
        icon_only = "if (IsValidIcon(item.icon) && !item.leadingIcon) {"
        self.assertIn(icon_only, self.renderer)
        branch = self.renderer.split(icon_only, 1)[1].split("if (button) {", 1)[0]
        self.assertIn("IconQuad(item.icon", branch)
        self.assertNotIn("LabelQuad(", branch)
        self.assertRegex(branch, r"continue;\s*}\s*$")
        self.assertIn("IconCaptionRect(*caption)", self.renderer)
        self.assertIn("IconCaptionRect(items[i]).Contains(point)", self.header)

    def test_shop_packet_guard_allows_three_playable_crops(self):
        types = source(ROOT / "project/application/farm/core/FarmTypes.h")
        self.assertRegex(types, r"kPlayableCropCount\s*=\s*3\s*;")
        branch = self.renderer.split("if (item.request.action == Action::ShopSelect", 1)[1]
        branch = branch.split("const int crop", 1)[0]
        self.assertRegex(branch, r"if\s*\(index\s*<\s*0\s*\|\|\s*index\s*>=\s*farm::kPlayableCropCount\)\s*continue;")
        self.assertNotRegex(branch, r"index\s*(?:>=\s*[12]|>\s*[01])\b")

    def test_toolbar_draw_requires_visible_hotbar(self):
        self.assertRegex(self.header, r"bool\s+hotbarVisible\s*=\s*false\s*;")
        self.assertRegex(
            self.renderer,
            r"if\s*\(view\.compactHUD\s*&&\s*view\.hotbarVisible\)\s*\{?\s*Panel\(HUDLayout::tools,",
        )
        self.assertEqual(self.renderer.count("Panel(HUDLayout::tools,"), 1)
        self.assertIn("view.hotbarVisible = true;", source(UI / "FarmHotbarView.h"))

    def test_crop_popup_build_skips_hotbar_and_resets_view(self):
        self.assertRegex(
            self.controller,
            r"void FarmRuntimeController::BuildView\([^)]*\)\s*\{\s*view_\s*=\s*\{\};",
        )
        self.assertRegex(
            self.controller,
            r"else if\s*\(!s\.farmCropSelectionSystem\.IsOpen\(\)\)\s*\{\s*farmui::BuildHotbarView\(",
        )
        self.assertEqual(self.controller.count("farmui::BuildHotbarView("), 1)
        self.assertNotIn("hotbarVisible = true", self.controller)

    def test_crop_popup_bottom_panel_matches_down_selection(self):
        selection = source(ROOT / "project/application/farm/system/FarmCropSelectionSystem.cpp")
        self.assertRegex(
            selection,
            r"if\s*\(deltaY\s*>\s*std::abs\(deltaX\)\)\s*hoveredCrop_\s*=\s*farm::CropType::Pumpkin;",
        )
        pie = self.hud.split("void FarmHUD::UpdateCropPieMenu()", 1)[1].split("void FarmHUD::Update(", 1)[0]
        self.assertIn("farm::PlayableCrop(static_cast<int>(i))", pie)
        self.assertIn("const std::size_t slot = i + 1;", pie)
        self.assertRegex(source(UI / "FarmHUD.h"), r"std::array<Sprite,\s*3>\s+cropPiePanels_")
        self.assertRegex(
            pie,
            r"center\.x\s*-\s*kPiePanelSize\.x\s*\*\s*0\.5f,\s*center\.y\s*\+\s*78",
        )
        self.assertRegex(self.hud, r"kPiePanelSize\s*\{174\.0f,\s*126\.0f\}")
        self.assertRegex(self.scene, r"virtualMouse\.y\s*=\s*std::clamp\(virtualMouse\.y,\s*130\.0f,\s*500\.0f\)")
        self.assertIn("farmCropSelectionSystem_.UpdatePointer(virtualMouse);", self.scene)
        self.assertIn("farmCropSelectionSystem_.Confirm();", self.scene)
        # These dimensions anchor the C++ popup/toolbar intersection fixture.
        layout = source(UI / "FarmHUDLayout.h")
        self.assertRegex(layout, r"Rect tools\s*\{380,\s*620,\s*520,\s*76\}")
        self.assertLess(500 + 78, 620 + 76)
        self.assertGreater(500 + 78 + 126, 620)

    def test_contest_target_projection_and_refresh(self):
        self.assertRegex(source(UI / "FarmHUD.h"), r"int\s+contestFinalDay\s*=\s*0\s*;")
        self.assertIn("viewData.contestFinalDay = FarmContestEntrySystem::kContestDays.back();", self.scene)
        self.assertIn("sanitized.contestFinalDay = (std::max)(sanitized.contestFinalDay, 0);", self.hud)
        changed = re.search(r"const bool goalChanged\s*=([^;]+);", self.hud)
        self.assertIsNotNone(changed)
        self.assertIn("statusChanged", changed.group(1))
        self.assertIn("viewData_.contestSeason != sanitized.contestSeason", changed.group(1))
        self.assertIn("viewData_.contestFinalDay != sanitized.contestFinalDay", changed.group(1))
        self.assertIn("if (goalChanged) UpdateGoal();", self.hud)
        goal = self.hud.split("void FarmHUD::UpdateGoal()", 1)[1].split("void FarmHUD::UpdateSelectedTile()", 1)[0]
        self.assertIn("if (!viewData_.freeFarming)", goal)
        self.assertIn("viewData_.contestSeason && viewData_.contestFinalDay > 0", goal)
        self.assertIn('"DAY %d / %d", (std::min)(viewData_.day, viewData_.contestFinalDay), viewData_.contestFinalDay', goal)
        self.assertIn('"DAY %d / ?", viewData_.day', goal)

    def test_goal_glyph_capacity_covers_full_int_range(self):
        initialize = self.hud.split("InitializationTask FarmHUD::InitializeText(", 1)[1].split("InitializationTask FarmHUD::", 1)[0]
        capacities = {
            name: int(size) for name, size in re.findall(
                r"(\w+Text_)\.Initialize\(common,\s*font_,\s*(\d+)\)", initialize
            )
        }
        self.assertEqual(capacities["goalText_"], 28)
        self.assertEqual(len("DAY 2147483647 / 2147483647"), 27)
        self.assertGreaterEqual(capacities["goalText_"], 27)
        for run, capacity in (("cropPieStatsText_", 23), ("cropPieValueText_", 11)):
            self.assertRegex(
                initialize,
                rf"for\s*\(auto& text\s*:\s*{run}\)\s*if\s*\(!co_await text\.Initialize\(common,\s*font_,\s*{capacity}\)\)",
            )
        self.assertEqual(sum(capacities.values()) + 3 * (23 + 11), 220)
        goal = self.hud.split("void FarmHUD::UpdateGoal()", 1)[1].split("void FarmHUD::UpdateSelectedTile()", 1)[0]
        buffer = re.search(r"char value\[(\d+)\]", goal)
        self.assertIsNotNone(buffer)
        self.assertGreaterEqual(int(buffer.group(1)), 28)

    def test_menu_atlas_policy_is_opt_in_and_cached_separately(self):
        texture_header = source(ROOT / "project/engine/2d/TextureManager.h")
        texture_source = source(ROOT / "project/engine/2d/TextureManager.cpp")
        self.assertIn("MipPolicy mipPolicy = MipPolicy::GenerateIfMissing", texture_header)
        self.assertIn('if (mipPolicy == MipPolicy::PreserveFile) key += L"|preserve-mips";', texture_source)
        self.assertIn("LoadImage(path, TextureKind::Texture2D, image, mipPolicy)", texture_source)
        self.assertIn("if (mipPolicy == MipPolicy::GenerateIfMissing && expectedKind", texture_source)
        self.assertIn("TextureManager::Lifetime::Scene, TextureManager::MipPolicy::PreserveFile", self.renderer)
        self.assertEqual(self.renderer.count("sprite.Initialize(common, atlas.Index())"), 2)
        self.assertEqual(self.renderer.count('"Resources/ui/farm_runtime_menu.png"'), 1)

    def test_glyph_initialization_yields_before_publishing_capacity(self):
        initialize = self.hud.split("InitializationTask FarmHUD::TextRun::Initialize(", 1)[1].split("void FarmHUD::TextRun::", 1)[0]
        self.assertIn("count = capacity = 0;", initialize)
        self.assertLess(initialize.index("co_await std::suspend_always{};"), initialize.index("capacity = maximum;"))
        self.assertIn("co_await InitializeFarmHUD()", self.scene)

    def test_growth_stage_refresh_and_unknown_fallback(self):
        changed = re.search(r"const bool selectedChanged\s*=([^;]+);", self.hud)
        self.assertIsNotNone(changed)
        self.assertIn("viewData_.selectedTileGrowthStage != sanitized.selectedTileGrowthStage", changed.group(1))
        self.assertIn("if (selectedChanged) UpdateSelectedTile();", self.hud)
        stages = re.search(r"switch\s*\(data.selectedTileGrowthStage\)\s*\{([^}]+)\}", self.hud)
        self.assertIsNotNone(stages)
        for stage, index in (("Sprout", 7), ("Growing", 8), ("AlmostReady", 9), ("Ready", 10)):
            self.assertRegex(stages.group(1), rf"case farm::FarmCropGrowthStage::{stage}:\s*return {index};")
        self.assertRegex(stages.group(1), r"default:\s*return 0;")

    def test_crop_request_equips_seed_only_outside_menu(self):
        controller = source(ROOT / "project/application/farm/system/FarmRuntimeController.cpp")
        branch = controller.split("case Action::Crop: {", 1)[1].split("case Action::Buy:", 1)[0]
        self.assertIn("request.argument >= farm::kFarmCropTypeCount", branch)
        self.assertIn("SetSelectedCrop(farm::CropTypeFromSlot(request.argument))", branch)
        self.assertRegex(branch, r"if\s*\(success\s*&&\s*!open_\)\s*s\.farmToolSystem\.SetTool\(FarmTool::Seed\)")
        self.assertNotIn("ApplyTool(", branch)
        self.assertIn("view_.selectedTile = hud.selectedTileValid;", controller)
        self.assertIn("view_.goalVisible = !hud.freeFarming;", controller)
        self.assertNotIn("View::FarmHUDCovers(", controller)


if __name__ == "__main__":
    unittest.main(verbosity=2)
