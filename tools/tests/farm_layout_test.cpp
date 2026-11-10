#include "farm/system/FarmLayoutSystem.h"
#include "externals/nlohmann/json.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    using namespace farm;
    const fs::path root = fs::path("generated/codex_checks") / ("layout_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    FarmLayoutSystem layouts;
    assert(layouts.Initialize(root.string()));
    FarmGrid grid; assert(grid.Initialize(5, 4));
    FarmTile raised{}; raised.heightLevel = 2; raised.state = FarmTileState::Planted;
    raised.crop = CropType::Carrot; raised.growth = 0.7f; raised.moisture = 0.8f;
    raised.irrigationEnabled = false;
    assert(grid.SetTile(0, raised));
    FarmTile source{}; source.feature = FarmTileFeature::WaterSource; source.heightLevel = 1; source.waterAmount = 0.2f;
    assert(grid.SetTile(1, source));
    FarmTile canal{}; canal.feature = FarmTileFeature::Canal; canal.waterAmount = 0.4f;
    assert(grid.SetTile(2, canal));
    assert(!layouts.SaveNew("", grid));
    assert(!layouts.SaveNew(" bad ", grid));
    assert(!layouts.SaveNew(std::string(193, 'a'), grid));
    assert(!layouts.SaveNew(std::string(1, static_cast<char>(0xff)), grid));
    assert(layouts.SaveNew("上段の水源・中央の畑", grid));
    assert(!layouts.SaveNew("上段の水源・中央の畑", grid));
    const auto id = layouts.Entries().front().id;
    const auto path = root / (id + ".json");
    nlohmann::json json; { std::ifstream file(path); file >> json; }
    assert(json.size() == 6 && json.at("tiles")[0].size() == 5 && json["version"] == 3);
    assert(!json.contains("economy") && !json.at("tiles")[0].contains("growth"));
    assert(grid.Initialize(5,4)); assert(grid.SetSelectedIndex(12));
    const auto generation = grid.GetGeneration();
    assert(layouts.Load(id, grid)); assert(grid.GetGeneration() > generation && grid.GetSelectedIndex() == 12);
    assert(grid.GetTile(0)->heightLevel == 2 && grid.GetTile(0)->state == FarmTileState::Tilled);
    assert(grid.GetTile(0)->crop == CropType::None && grid.GetTile(0)->growth == 0 && grid.GetTile(0)->moisture == 0);
    assert(!grid.GetTile(0)->irrigationEnabled);
    assert(grid.GetTile(1)->feature == FarmTileFeature::WaterSource && grid.GetTile(1)->waterAmount == 1);
    assert(grid.GetTile(2)->feature == FarmTileFeature::Canal && grid.GetTile(2)->waterAmount == 0);
    FarmLayoutSystem reopened; assert(reopened.Initialize(root.string()));
    assert(reopened.Entries().size() == 1 && reopened.Entries()[0].name == "上段の水源・中央の畑");
    const auto stable = grid.GetGeneration();
    assert(!layouts.Load("../progress", grid));
    const auto reject = [&](nlohmann::json value) {
        { std::ofstream file(path); file << value.dump(); }
        assert(!layouts.Load(id, grid));
        assert(grid.GetGeneration() == stable && grid.GetTile(0)->heightLevel == 2);
    };
    auto invalid = json; invalid["width"] = 4; reject(invalid);
    invalid = json; invalid["kind"] = "farm_document"; reject(invalid);
    invalid = json; invalid["version"] = 999; reject(invalid);
    invalid = json; invalid["tiles"][0]["height"] = 3; reject(invalid);
    invalid = json; invalid["tiles"][0]["height"] = 1.5; reject(invalid);
    invalid = json; invalid["tiles"][0]["height"] = 18446744073709551615ull; reject(invalid);
    invalid = json; invalid["tiles"][0]["feature"] = 90; reject(invalid);
    invalid = json; invalid["tiles"][1]["cultivated"] = true; reject(invalid);
    invalid = json; invalid["tiles"].erase(0); reject(invalid);
    invalid = json; invalid["tiles"][0].erase("irrigationEnabled"); reject(invalid);
    for (const auto& value : {nlohmann::json(nullptr), nlohmann::json(1), nlohmann::json("false"), nlohmann::json::array()}) {
        invalid = json; invalid["tiles"][0]["irrigationEnabled"] = value; reject(invalid);
        assert(!grid.GetTile(0)->irrigationEnabled);
    }
    { std::ofstream file(path); file << "{"; }
    assert(!layouts.Load(id, grid) && grid.GetGeneration() == stable);
    { std::ofstream file(path); file << std::string(1024 * 1024 + 1, ' '); }
    assert(!layouts.Load(id, grid) && grid.GetGeneration() == stable);
    { std::ofstream file(path); file << json.dump(); }
    assert(layouts.Load(id, grid));
    auto legacy = json; legacy["version"] = 1;
    for (auto& tile : legacy["tiles"]) tile.erase("irrigationEnabled");
    { std::ofstream file(path); file << legacy.dump(); }
    assert(layouts.Load(id, grid) && grid.GetTile(0)->irrigationEnabled);
    std::cout << "farm_layout_checks=passed\n";
}
