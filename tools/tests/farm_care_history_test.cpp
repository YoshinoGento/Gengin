#include "farm/core/FarmGrid.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "io/JsonFile.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool Near(float left, float right) noexcept
{
	return std::fabs(left - right) < 0.0001f;
}

void VerifyHistory(
	const farm::FarmCropCareHistory& actual,
	const farm::FarmCropCareHistory& expected)
{
	assert(Near(actual.drySeconds, expected.drySeconds));
	assert(Near(actual.lowSeconds, expected.lowSeconds));
	assert(Near(actual.goodSeconds, expected.goodSeconds));
	assert(Near(actual.excessSeconds, expected.excessSeconds));
	assert(Near(actual.efficiencySeconds, expected.efficiencySeconds));
	assert(Near(actual.nutrientGrowth, expected.nutrientGrowth));
	assert(Near(actual.nutrientSupply, expected.nutrientSupply));
}

} // namespace

int main()
{
	const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
	const std::filesystem::path root =
		std::filesystem::temp_directory_path() /
		("cg2_farm_care_history_" + std::to_string(suffix));
	const std::filesystem::path legacyRoot = root / "legacy";
	std::error_code error;
	std::filesystem::create_directories(root, error);
	assert(!error);

	farm::FarmGrid grid;
	FarmEconomySystem economy;
	FarmCropSelectionSystem cropSelection;
	assert(grid.Initialize(2, 1));
	economy.Initialize();
	cropSelection.Initialize();

	farm::FarmTile crop;
	crop.state = farm::FarmTileState::Planted;
	crop.crop = farm::CropType::Carrot;
	crop.moisture = 0.62f;
	crop.growth = 0.75f;
	crop.careHistory.drySeconds = 1.0f;
	crop.careHistory.lowSeconds = 2.0f;
	crop.careHistory.goodSeconds = 6.0f;
	crop.careHistory.excessSeconds = 1.0f;
	crop.careHistory.efficiencySeconds = 8.25f;
	crop.soilNutrients = 0.32f;
	crop.careHistory.nutrientGrowth = 0.70f;
	crop.careHistory.nutrientSupply = 0.45f;
	assert(grid.SetTile(0, crop));
	farm::FarmTile invalidHistory = crop;
	invalidHistory.careHistory.efficiencySeconds = 20.0f;
	assert(!grid.SetTile(1, invalidHistory));
	invalidHistory = {};
	invalidHistory.careHistory.goodSeconds = 1.0f;
	invalidHistory.careHistory.efficiencySeconds = 1.0f;
	assert(!grid.SetTile(1, invalidHistory));

	FarmDocumentSystem documents;
	FarmCropQualitySystem qualitySystem;
	qualitySystem.Initialize();
	const auto harvested = qualitySystem.Evaluate(crop);
	assert(economy.AddHarvest(harvested, 1));
	assert(documents.Initialize(root.string(), grid, economy, cropSelection));
	assert(documents.SaveAs("Care History", grid, economy, cropSelection));
	const std::string savedId = documents.GetActiveDocumentId();
	const std::string savedPath = documents.GetPath();
	assert(!savedId.empty() && !savedPath.empty());

	nlohmann::json savedJson;
	assert(JsonFile::Load(savedPath, savedJson));
    assert(savedJson["schemaVersion"].get<int>() == 25);
	assert(savedJson["tiles"][0]["careHistory"].is_object());
	assert(Near(
		savedJson["tiles"][0]["careHistory"]["efficiencySeconds"].get<float>(),
		crop.careHistory.efficiencySeconds));

	farm::FarmTile changed = crop;
	changed.careHistory = {};
	assert(grid.SetTile(0, changed));
	assert(documents.Load(savedId, grid, economy, cropSelection));
	assert(grid.GetTile(0) != nullptr);
	VerifyHistory(grid.GetTile(0)->careHistory, crop.careHistory);
	assert(grid.GetTile(0)->soilNutrients == crop.soilNutrients);
	assert(economy.GetLastHarvestQuality().nutrientKnown);
	assert(Near(economy.GetLastHarvestQuality().nutrientBalance, harvested.nutrientBalance));
	for (const auto bad : {-0.1f, 1.1f}) {
		auto invalid = savedJson;
		invalid["tiles"][0]["soilNutrients"] = bad;
		assert(JsonFile::Save(savedPath, invalid));
		assert(!documents.Load(savedId, grid, economy, cropSelection));
		assert(grid.GetTile(0)->soilNutrients == crop.soilNutrients);
	}
	assert(JsonFile::Save(savedPath, savedJson));

	nlohmann::json legacyJson = savedJson;
	legacyJson["schemaVersion"] = 5;
    for (const char* field : {"cropCounts", "cropValues", "seedCounts"}) {
        legacyJson["economy"][field] = nlohmann::json::array({savedJson["economy"][field][0], savedJson["economy"][field][1]});
    }
	legacyJson["document"]["id"] = "legacy";
	legacyJson["document"]["displayName"] = "Legacy Care";
	for (auto& tile : legacyJson["tiles"]) {
		tile.erase("careHistory");
		tile.erase("soilNutrients");
	}
	std::filesystem::create_directories(legacyRoot / "saves", error);
	assert(!error);
	assert(JsonFile::Save((legacyRoot / "saves" / "legacy.json").string(), legacyJson));

	farm::FarmGrid legacyGrid;
	FarmEconomySystem legacyEconomy;
	FarmCropSelectionSystem legacyCropSelection;
	assert(legacyGrid.Initialize(2, 1));
	legacyEconomy.Initialize();
	legacyCropSelection.Initialize();
	FarmDocumentSystem legacyDocuments;
	assert(legacyDocuments.Initialize(
		legacyRoot.string(), legacyGrid, legacyEconomy, legacyCropSelection));
	assert(legacyDocuments.GetActiveDocumentId() == "legacy");
	assert(legacyGrid.GetTile(0) != nullptr &&
		legacyGrid.GetTile(0)->careHistory.IsEmpty());
	assert(Near(legacyGrid.GetTile(0)->soilNutrients, 0.60f));
	assert(!legacyEconomy.GetLastHarvestQuality().nutrientKnown);
	assert(legacyEconomy.GetLastHarvestQuality().score == harvested.score);
	legacyJson = savedJson;
	legacyJson["schemaVersion"] = 6;
    for (const char* field : {"cropCounts", "cropValues", "seedCounts"}) {
        legacyJson["economy"][field] = nlohmann::json::array({savedJson["economy"][field][0], savedJson["economy"][field][1]});
    }
	legacyJson["document"]["id"] = "legacy";
	for (auto& tile : legacyJson["tiles"]) {
		tile.erase("soilNutrients");
		tile["careHistory"].erase("nutrientGrowth");
		tile["careHistory"].erase("nutrientSupply");
	}
	assert(JsonFile::Save((legacyRoot / "saves" / "legacy.json").string(), legacyJson));
	assert(legacyDocuments.Load("legacy", legacyGrid, legacyEconomy, legacyCropSelection));
	assert(Near(legacyGrid.GetTile(0)->soilNutrients, 0.60f));
	assert(legacyGrid.GetTile(0)->careHistory.nutrientGrowth == 0);
	assert(Near(legacyGrid.GetTile(0)->careHistory.goodSeconds, crop.careHistory.goodSeconds));

	std::filesystem::remove_all(root, error);
	assert(!error);
	std::cout << "PASS: schema 25 soil/history round trip, invalid input, schema 5/6 migration\n";
}
