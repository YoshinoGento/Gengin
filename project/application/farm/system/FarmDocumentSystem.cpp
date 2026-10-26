#include "farm/system/FarmDocumentSystem.h"

#include "base/Logger.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolActionSystem.h"
#include "io/JsonFile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
constexpr int kSchemaVersion = 18;
constexpr int kMinimumSupportedSchemaVersion = 1;
constexpr int kCatalogSchemaVersion = 1;
constexpr int kMaximumGridDimension = 128;
constexpr std::size_t kMaximumDisplayNameBytes = 192;
constexpr float kMinimumNormalizedValue = 0.0f;
constexpr float kMaximumNormalizedValue = 1.0f;
constexpr std::string_view kUntitledFarmName = "Untitled Farm";

bool TryGetSchemaVersion(const nlohmann::json& document, int& output) {
	if (!document.is_object() || !document.contains("schemaVersion") ||
		!document["schemaVersion"].is_number_integer() ||
		document["schemaVersion"] < kMinimumSupportedSchemaVersion ||
		document["schemaVersion"] > kSchemaVersion) {
		return false;
	}
	output = document["schemaVersion"].get<int>();
	return output >= kMinimumSupportedSchemaVersion && output <= kSchemaVersion;
}

bool TryParseState(const std::string& value, farm::FarmTileState& output) {
	if (value == "Empty") {
		output = farm::FarmTileState::Empty;
		return true;
	}
	if (value == "Tilled") {
		output = farm::FarmTileState::Tilled;
		return true;
	}
	if (value == "Planted") {
		output = farm::FarmTileState::Planted;
		return true;
	}
	return false;
}

bool TryParseCrop(const std::string& value, farm::CropType& output, int schemaVersion) {
	if (value == "None") {
		output = farm::CropType::None;
		return true;
	}
	if (value == "TestCrop") {
		output = farm::CropType::TestCrop;
		return true;
	}
	if (value == "Carrot") {
		output = farm::CropType::Carrot;
		return true;
	}
	if (schemaVersion >= 16 && value == "Tomato") { output = farm::CropType::Tomato; return true; }
	if (schemaVersion >= 16 && value == "Pumpkin") { output = farm::CropType::Pumpkin; return true; }
	return false;
}

bool TryParseFeature(const std::string& value, farm::FarmTileFeature& output) {
	if (value == "None") {
		output = farm::FarmTileFeature::None;
		return true;
	}
	if (value == "Canal") {
		output = farm::FarmTileFeature::Canal;
		return true;
	}
	if (value == "WaterSource") {
		output = farm::FarmTileFeature::WaterSource;
		return true;
	}
	return false;
}

bool ValidateTile(const farm::FarmTile& tile, std::string& error) {
	if (tile.heightLevel < FarmToolActionSystem::kMinimumHeightLevel ||
		tile.heightLevel > FarmToolActionSystem::kMaximumHeightLevel) {
		error = "Tile height is outside the supported range.";
		return false;
	}
	if (!farm::IsValidFarmTileFeature(tile.feature)) {
		error = "Tile feature is unsupported.";
		return false;
	}
	if (!std::isfinite(tile.waterAmount) || tile.waterAmount < 0.0f || tile.waterAmount > 1.0f ||
		(tile.feature == farm::FarmTileFeature::None && tile.waterAmount != 0.0f)) {
		error = "Reservoir water is invalid or stored on a soil tile.";
		return false;
	}
	if (!std::isfinite(tile.moisture) || !std::isfinite(tile.growth) ||
		!tile.careHistory.IsValid() || !std::isfinite(tile.soilNutrients) ||
		tile.soilNutrients < 0.0f || tile.soilNutrients > 1.0f ||
		tile.moisture < kMinimumNormalizedValue || tile.moisture > kMaximumNormalizedValue ||
		tile.growth < kMinimumNormalizedValue || tile.growth > kMaximumNormalizedValue) {
		error = "Tile moisture or growth is invalid.";
		return false;
	}
	if (tile.state == farm::FarmTileState::Planted) {
		if (tile.crop == farm::CropType::None) {
			error = "A planted tile must contain a crop.";
			return false;
		}
	} else if (tile.crop != farm::CropType::None || tile.growth != 0.0f ||
		!tile.careHistory.IsEmpty()) {
		error = "Only planted tiles may contain crop growth data.";
		return false;
	}
	if (tile.feature != farm::FarmTileFeature::None &&
		(tile.state != farm::FarmTileState::Empty ||
		 tile.crop != farm::CropType::None || tile.moisture != 0.0f || tile.growth != 0.0f)) {
		error = "A Farm infrastructure tile cannot contain cultivation data.";
		return false;
	}
	return true;
}

bool ValidateSnapshot(
	const farm::FarmGrid::Snapshot& snapshot,
	int expectedWidth,
	int expectedHeight,
	std::string& error) {
	if (snapshot.width <= 0 || snapshot.height <= 0 ||
		snapshot.width > kMaximumGridDimension || snapshot.height > kMaximumGridDimension) {
		error = "Farm grid dimensions are invalid.";
		return false;
	}
	if (snapshot.width != expectedWidth || snapshot.height != expectedHeight) {
		error = "Farm grid dimensions do not match the current scene.";
		return false;
	}
	const std::size_t expectedTileCount =
		static_cast<std::size_t>(snapshot.width) * static_cast<std::size_t>(snapshot.height);
	if (snapshot.tiles.size() != expectedTileCount) {
		error = "Farm tile count does not match the grid dimensions.";
		return false;
	}
	if (snapshot.selectedX < 0 || snapshot.selectedX >= snapshot.width ||
		snapshot.selectedY < 0 || snapshot.selectedY >= snapshot.height) {
		error = "Selected Farm tile is outside the grid.";
		return false;
	}
	for (const farm::FarmTile& tile : snapshot.tiles) {
		if (!ValidateTile(tile, error)) {
			return false;
		}
	}
	return true;
}

bool IsValidUtf8(std::string_view value) {
	std::size_t index = 0;
	while (index < value.size()) {
		const auto lead = static_cast<unsigned char>(value[index]);
		std::size_t continuationCount = 0;
		std::uint32_t codePoint = 0;
		if (lead <= 0x7f) {
			continuationCount = 0;
			codePoint = lead;
		} else if ((lead & 0xe0u) == 0xc0u) {
			continuationCount = 1;
			codePoint = lead & 0x1fu;
		} else if ((lead & 0xf0u) == 0xe0u) {
			continuationCount = 2;
			codePoint = lead & 0x0fu;
		} else if ((lead & 0xf8u) == 0xf0u) {
			continuationCount = 3;
			codePoint = lead & 0x07u;
		} else {
			return false;
		}
		if (index + continuationCount >= value.size()) {
			return false;
		}
		for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
			const auto continuation = static_cast<unsigned char>(value[index + offset]);
			if ((continuation & 0xc0u) != 0x80u) {
				return false;
			}
			codePoint = (codePoint << 6u) | (continuation & 0x3fu);
		}
		const bool overlong =
			(continuationCount == 1 && codePoint < 0x80u) ||
			(continuationCount == 2 && codePoint < 0x800u) ||
			(continuationCount == 3 && codePoint < 0x10000u);
		if (overlong || codePoint > 0x10ffffu ||
			(codePoint >= 0xd800u && codePoint <= 0xdfffu)) {
			return false;
		}
		index += continuationCount + 1;
	}
	return true;
}

bool NormalizeDisplayName(const std::string& value, std::string& output, std::string& error) {
	const std::size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		error = "Farm name cannot be empty.";
		return false;
	}
	const std::size_t last = value.find_last_not_of(" \t\r\n");
	output = value.substr(first, last - first + 1);
	if (output.size() > kMaximumDisplayNameBytes) {
		error = "Farm name is too long.";
		return false;
	}
	if (!IsValidUtf8(output)) {
		error = "Farm name is not valid UTF-8.";
		return false;
	}
	for (const unsigned char character : output) {
		if (character < 0x20u || character == 0x7fu) {
			error = "Farm name contains a control character.";
			return false;
		}
	}
	return true;
}

bool IsSafeDocumentId(std::string_view value) {
	if (value.empty() || value.size() > 96) {
		return false;
	}
	return std::all_of(value.begin(), value.end(), [](char character) {
		return (character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') ||
			character == '_' || character == '-';
	});
}

std::string MakeSavedAtTimestamp() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime{};
	if (localtime_s(&localTime, &nowTime) != 0) {
		return "Unknown time";
	}
	std::ostringstream stream;
	stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	return stream.str();
}

std::string GenerateDocumentId(const std::string& saveDirectoryPath) {
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	for (int suffix = 0; suffix < 1000; ++suffix) {
		const std::string id = "farm_" + std::to_string(milliseconds) + "_" + std::to_string(suffix);
		const std::filesystem::path path = std::filesystem::path(saveDirectoryPath) / (id + ".json");
		if (!JsonFile::Exists(path.string())) {
			return id;
		}
	}
	return {};
}

nlohmann::json BuildQualityJson(const FarmCropQualityResult& quality) {
	return {
			{ "crop", farm::ToString(quality.crop) },
			{ "maturity", quality.maturity },
			{ "waterBalance", quality.waterBalance },
			{ "terrainFit", quality.terrainFit },
			{ "nutrientBalance", quality.nutrientBalance },
			{ "nutrientKnown", quality.nutrientKnown },
			{ "sizeMultiplier", quality.harvestSize.multiplier },
			{ "sizeKnown", quality.harvestSize.known },
			{ "score", quality.score },
			{ "basePrice", quality.basePrice },
			{ "salePrice", quality.salePrice },
		};
}

nlohmann::json BuildJson(
	const farm::FarmGrid::Snapshot& snapshot,
	const FarmEconomySystem::Snapshot& economySnapshot,
	const FarmCropSelectionSystem::Snapshot& cropSelectionSnapshot,
	const std::string& documentId,
	const std::string& displayName,
	const std::string& savedAt) {
	nlohmann::json document;
	document["schemaVersion"] = kSchemaVersion;
	document["document"] = {
		{ "id", documentId },
		{ "displayName", displayName },
		{ "savedAt", savedAt },
	};
	document["grid"] = {
		{ "width", snapshot.width },
		{ "height", snapshot.height },
		{ "selectedIndex", snapshot.selectedY * snapshot.width + snapshot.selectedX },
	};
	document["tiles"] = nlohmann::json::array();
	for (const farm::FarmTile& tile : snapshot.tiles) {
		document["tiles"].push_back({
			{ "height", tile.heightLevel },
			{ "feature", farm::ToString(tile.feature) },
			{ "state", farm::ToString(tile.state) },
			{ "crop", farm::ToString(tile.crop) },
			{ "moisture", tile.moisture },
			{ "growth", tile.growth },
			{ "waterAmount", tile.waterAmount },
			{ "irrigationEnabled", tile.irrigationEnabled },
			{ "soilNutrients", tile.soilNutrients },
			{ "careHistory", {
				{ "drySeconds", tile.careHistory.drySeconds },
				{ "lowSeconds", tile.careHistory.lowSeconds },
				{ "goodSeconds", tile.careHistory.goodSeconds },
				{ "excessSeconds", tile.careHistory.excessSeconds },
				{ "efficiencySeconds", tile.careHistory.efficiencySeconds },
				{ "nutrientGrowth", tile.careHistory.nutrientGrowth },
				{ "nutrientSupply", tile.careHistory.nutrientSupply },
			} },
		});
	}
	document["economy"] = {
		{ "money", economySnapshot.money },
		{ "cropCounts", economySnapshot.cropCounts },
		{ "cropValues", economySnapshot.cropValues },
		{ "seedCounts", economySnapshot.seedCounts },
		{ "nextHarvestRecordId", economySnapshot.nextHarvestRecordId },
		{ "contestReservationId", economySnapshot.contestReservationId },
	};
	document["economy"]["lastHarvestQuality"] = economySnapshot.lastHarvestQuality.crop == farm::CropType::None
		? nlohmann::json(nullptr) : BuildQualityJson(economySnapshot.lastHarvestQuality);
	auto& records = document["economy"]["harvestRecords"] = nlohmann::json::array();
	for (std::size_t i = 0; i < economySnapshot.harvestRecordCount; ++i) {
		const auto& record = economySnapshot.harvestRecords[i];
		records.push_back({{"quantity", record.quantity}, {"quality", BuildQualityJson(record.quality)},
			{"id", record.id}, {"saleProtected", record.saleProtected}, {"harvestedDay", record.harvestedDay}});
	}

	auto& submissions = document["economy"]["contestResults"] = nlohmann::json::array();
	for (const auto& result : economySnapshot.contestResults) {
		if (result.contestDay == 0) continue;
		submissions.push_back({{"contestDay", result.contestDay}, {"rulesVersion", 1},
			{"recordId", result.harvest.id}, {"harvestedDay", result.harvest.harvestedDay},
			{"quality", BuildQualityJson(result.harvest.quality)},
			{"qualityPoints", result.qualityPoints}, {"sizePoints", result.sizePoints}});
	}
	document["cropSelection"] = {
		{ "crop", farm::ToString(cropSelectionSnapshot.selectedCrop) },
	};
	return document;
}

// Only validated load candidates are migrated; stored IDs and original files stay readable.
bool MigrateLegacyCrops(farm::FarmGrid::Snapshot& grid, FarmEconomySystem::Snapshot& economy,
	FarmCropSelectionSystem::Snapshot& selection, bool& changed) {
	changed = false;
	const auto legacy = static_cast<std::size_t>(farm::ToCropSlot(farm::CropType::TestCrop));
	const auto carrot = static_cast<std::size_t>(farm::ToCropSlot(farm::CropType::Carrot));
	for (const auto* counts : {&economy.seedCounts, &economy.cropCounts, &economy.cropValues}) {
		if ((*counts)[legacy] > (std::numeric_limits<int>::max)() - (*counts)[carrot]) return false;
	}
	for (auto* counts : {&economy.seedCounts, &economy.cropCounts, &economy.cropValues}) {
		changed |= (*counts)[legacy] != 0;
		(*counts)[carrot] += (*counts)[legacy];
		(*counts)[legacy] = 0;
	}
	const auto migrate = [&changed](farm::CropType& crop) {
		if (crop == farm::CropType::TestCrop) { crop = farm::CropType::Carrot; changed = true; }
	};
	for (auto& tile : grid.tiles) migrate(tile.crop);
	migrate(selection.selectedCrop);
	migrate(economy.lastHarvestQuality.crop);
	for (std::size_t i = 0; i < economy.harvestRecordCount; ++i) migrate(economy.harvestRecords[i].quality.crop);
	for (auto& result : economy.contestResults) {
		if (result.contestDay != 0) migrate(result.harvest.quality.crop);
	}
	// Historical sale prices and confirmed points are not recalculated on migration.
	return FarmEconomySystem::ValidateSnapshot(economy);
}

bool ParseMetadata(
	const nlohmann::json& document,
	std::string_view expectedId,
	FarmDocumentEntry& output,
	std::string& error) {
	try {
		int schemaVersion = 0;
		if (!TryGetSchemaVersion(document, schemaVersion) ||
			!document.contains("document") || !document["document"].is_object()) {
			error = "Unsupported Farm document metadata.";
			return false;
		}
		static_cast<void>(schemaVersion);
		const nlohmann::json& metadata = document["document"];
		if (!metadata.contains("id") || !metadata["id"].is_string() ||
			!metadata.contains("displayName") || !metadata["displayName"].is_string() ||
			!metadata.contains("savedAt") || !metadata["savedAt"].is_string()) {
			error = "Farm document metadata has invalid types.";
			return false;
		}
		output.id = metadata["id"].get<std::string>();
		output.displayName = metadata["displayName"].get<std::string>();
		output.savedAt = metadata["savedAt"].get<std::string>();
		std::string normalizedName;
		if (!IsSafeDocumentId(output.id) || output.id != expectedId ||
			!NormalizeDisplayName(output.displayName, normalizedName, error)) {
			if (error.empty()) {
				error = "Farm document ID does not match its filename.";
			}
			return false;
		}
		output.displayName = std::move(normalizedName);
		return true;
	} catch (const std::exception& exception) {
		error = std::string("Farm metadata parse failed: ") + exception.what();
		return false;
	}
}

bool ParseSnapshot(
	const nlohmann::json& document,
	int expectedWidth,
	int expectedHeight,
	farm::FarmGrid::Snapshot& output,
	std::string& error) {
	try {
		int schemaVersion = 0;
		if (!TryGetSchemaVersion(document, schemaVersion)) {
			error = "Unsupported or missing Farm schema version.";
			return false;
		}
		if (!document.contains("grid") || !document["grid"].is_object() ||
			!document.contains("tiles") || !document["tiles"].is_array()) {
			error = "Farm document is missing grid or tiles data.";
			return false;
		}

		const nlohmann::json& gridJson = document["grid"];
		if (!gridJson.contains("width") || !gridJson["width"].is_number_integer() ||
			!gridJson.contains("height") || !gridJson["height"].is_number_integer() ||
			!gridJson.contains("selectedIndex") || !gridJson["selectedIndex"].is_number_integer()) {
			error = "Farm grid metadata has invalid types.";
			return false;
		}

		farm::FarmGrid::Snapshot candidate;
		candidate.width = gridJson["width"].get<int>();
		candidate.height = gridJson["height"].get<int>();
		const int selectedIndex = gridJson["selectedIndex"].get<int>();
		if (candidate.width <= 0 || selectedIndex < 0) {
			error = "Farm grid metadata is outside the supported range.";
			return false;
		}
		candidate.selectedX = selectedIndex % candidate.width;
		candidate.selectedY = selectedIndex / candidate.width;
		candidate.tiles.reserve(document["tiles"].size());

		for (const nlohmann::json& tileJson : document["tiles"]) {
			if (!tileJson.is_object() ||
				!tileJson.contains("height") || !tileJson["height"].is_number_integer() ||
				!tileJson.contains("state") || !tileJson["state"].is_string() ||
				!tileJson.contains("crop") || !tileJson["crop"].is_string() ||
				!tileJson.contains("moisture") || !tileJson["moisture"].is_number() ||
				!tileJson.contains("growth") || !tileJson["growth"].is_number()) {
				error = "Farm tile data has missing fields or invalid types.";
				return false;
			}

			farm::FarmTile tile;
			tile.heightLevel = tileJson["height"].get<int>();
			tile.moisture = tileJson["moisture"].get<float>();
			if (schemaVersion >= 15) {
				if (!tileJson.contains("irrigationEnabled") || !tileJson["irrigationEnabled"].is_boolean()) {
					error = "Farm tile has missing or invalid irrigation intake.";
					return false;
				}
				tile.irrigationEnabled = tileJson["irrigationEnabled"].get<bool>();
			}
			tile.growth = tileJson["growth"].get<float>();
			if (schemaVersion >= 5) {
				if (!tileJson.contains("waterAmount") || !tileJson["waterAmount"].is_number()) {
					error = "Farm tile has missing or invalid reservoir water.";
					return false;
				}
				tile.waterAmount = tileJson["waterAmount"].get<float>();
			}
			if (schemaVersion >= 6) {
				if (!tileJson.contains("careHistory") ||
					!tileJson["careHistory"].is_object()) {
					error = "Farm tile has missing or invalid crop-care history.";
					return false;
				}
				const nlohmann::json& historyJson = tileJson["careHistory"];
				if (!historyJson.contains("drySeconds") || !historyJson["drySeconds"].is_number() ||
					!historyJson.contains("lowSeconds") || !historyJson["lowSeconds"].is_number() ||
					!historyJson.contains("goodSeconds") || !historyJson["goodSeconds"].is_number() ||
					!historyJson.contains("excessSeconds") || !historyJson["excessSeconds"].is_number() ||
					!historyJson.contains("efficiencySeconds") || !historyJson["efficiencySeconds"].is_number()) {
					error = "Farm crop-care history has missing fields or invalid types.";
					return false;
				}
				tile.careHistory.drySeconds = historyJson["drySeconds"].get<float>();
				tile.careHistory.lowSeconds = historyJson["lowSeconds"].get<float>();
				tile.careHistory.goodSeconds = historyJson["goodSeconds"].get<float>();
				tile.careHistory.excessSeconds = historyJson["excessSeconds"].get<float>();
				tile.careHistory.efficiencySeconds = historyJson["efficiencySeconds"].get<float>();
			}
			if (schemaVersion >= 7) {
				if (!tileJson.contains("soilNutrients") || !tileJson["soilNutrients"].is_number() ||
					!tileJson["careHistory"].contains("nutrientGrowth") || !tileJson["careHistory"]["nutrientGrowth"].is_number() ||
					!tileJson["careHistory"].contains("nutrientSupply") || !tileJson["careHistory"]["nutrientSupply"].is_number()) {
					error = "Missing or invalid soil nutrient data.";
					return false;
				}
				tile.soilNutrients = tileJson["soilNutrients"].get<float>();
				tile.careHistory.nutrientGrowth = tileJson["careHistory"]["nutrientGrowth"].get<float>();
				tile.careHistory.nutrientSupply = tileJson["careHistory"]["nutrientSupply"].get<float>();
			}
			if (!TryParseState(tileJson["state"].get<std::string>(), tile.state) ||
				!TryParseCrop(tileJson["crop"].get<std::string>(), tile.crop, schemaVersion)) {
				error = "Farm tile contains an unsupported state or crop.";
				return false;
			}
			if (schemaVersion >= 3) {
				if (!tileJson.contains("feature") || !tileJson["feature"].is_string() ||
					!TryParseFeature(tileJson["feature"].get<std::string>(), tile.feature)) {
					error = "Farm tile contains an unsupported feature.";
					return false;
				}
			}
			candidate.tiles.push_back(tile);
		}

		if (!ValidateSnapshot(candidate, expectedWidth, expectedHeight, error)) {
			return false;
		}
		output = std::move(candidate);
		return true;
	} catch (const std::exception& exception) {
		error = std::string("Farm document parse failed: ") + exception.what();
		return false;
	}
}

template<std::size_t Size>
bool ParseNonNegativeIntArray(
	const nlohmann::json& object, const char* key,
	std::array<int, Size>& output, std::string& error, int schemaVersion) {
	const std::size_t expected = schemaVersion < 16 ? 2 : Size;
	if (!object.contains(key) || !object[key].is_array() || object[key].size() != expected) {
		error = std::string("Farm economy field has an invalid size: ") + key;
		return false;
	}
	output.fill(0);
	for (std::size_t index = 0; index < expected; ++index) {
		if (!object[key][index].is_number_integer() || object[key][index] < 0 ||
			object[key][index] > (std::numeric_limits<int>::max)()) {
			error = std::string("Farm economy field has an invalid type: ") + key;
			return false;
		}
		const int value = object[key][index].get<int>();
		if (value < 0) {
			error = std::string("Farm economy field contains a negative value: ") + key;
			return false;
		}
		output[index] = value;
	}
	return true;
}

bool ReadNonNegativeInt(const nlohmann::json& value, int& output) {
	if (!value.is_number_integer() || value < 0 || value > (std::numeric_limits<int>::max)()) return false;
	output = value.get<int>();
	return true;
}

bool ParseQualityJson(const nlohmann::json& qualityJson, int schemaVersion,
	FarmCropQualityResult& output, std::string& error) {
	if (!qualityJson.is_object() ||
		!qualityJson.contains("crop") || !qualityJson["crop"].is_string() ||
		!qualityJson.contains("maturity") || !qualityJson["maturity"].is_number() ||
		!qualityJson.contains("waterBalance") || !qualityJson["waterBalance"].is_number() ||
		!qualityJson.contains("terrainFit") || !qualityJson["terrainFit"].is_number() ||
		!qualityJson.contains("score") || !qualityJson["score"].is_number_integer() ||
		!qualityJson.contains("basePrice") || !qualityJson["basePrice"].is_number_integer() ||
		!qualityJson.contains("salePrice") || !qualityJson["salePrice"].is_number_integer()) {
		error = "Farm last-harvest quality has invalid fields.";
		return false;
	}
	FarmCropQualityResult quality;
	quality.maturity = qualityJson["maturity"].get<float>();
	quality.waterBalance = qualityJson["waterBalance"].get<float>();
	quality.terrainFit = qualityJson["terrainFit"].get<float>();
	if (schemaVersion >= 7) {
		if (!qualityJson.contains("nutrientBalance") || !qualityJson["nutrientBalance"].is_number() ||
			!qualityJson.contains("nutrientKnown") || !qualityJson["nutrientKnown"].is_boolean()) {
			error = "Missing or invalid nutrient quality.";
			return false;
		}
		quality.nutrientBalance = qualityJson["nutrientBalance"].get<float>();
		quality.nutrientKnown = qualityJson["nutrientKnown"].get<bool>();
	}
	if (!ReadNonNegativeInt(qualityJson["score"], quality.score) ||
		!ReadNonNegativeInt(qualityJson["basePrice"], quality.basePrice) ||
		!ReadNonNegativeInt(qualityJson["salePrice"], quality.salePrice)) {
		error = "Quality integer fields are outside the supported range."; return false;
	}
	if (schemaVersion >= 8) {
		if (!qualityJson.contains("sizeMultiplier") || !qualityJson["sizeMultiplier"].is_number() ||
			!qualityJson.contains("sizeKnown") || !qualityJson["sizeKnown"].is_boolean()) {
			error = "Missing or invalid crop size record.";
			return false;
		}
		quality.harvestSize = {qualityJson["sizeMultiplier"].get<float>(), qualityJson["sizeKnown"].get<bool>()};
	}
	if (!TryParseCrop(qualityJson["crop"].get<std::string>(), quality.crop, schemaVersion) ||
		!quality.IsValid() || !std::isfinite(quality.maturity) ||
		!std::isfinite(quality.nutrientBalance) || quality.nutrientBalance < 0.0f || quality.nutrientBalance > 1.0f ||
		!std::isfinite(quality.waterBalance) || !std::isfinite(quality.terrainFit) ||
		quality.maturity < 0.0f || quality.maturity > 1.0f ||
		quality.waterBalance < 0.0f || quality.waterBalance > 1.0f ||
		quality.terrainFit < 0.0f || quality.terrainFit > 1.0f ||
		quality.score < 0 || quality.score > 100) {
		error = "Farm last-harvest quality is outside the supported range.";
		return false;
	}
	output = quality;
	return true;
}

bool ParsePersistentState(
	const nlohmann::json& document,
	FarmEconomySystem::Snapshot& economySnapshot,
	FarmCropSelectionSystem::Snapshot& cropSelectionSnapshot,
	std::string& error) {
	try {
		int schemaVersion = 0;
		if (!TryGetSchemaVersion(document, schemaVersion)) {
			error = "Unsupported or missing Farm schema version.";
			return false;
		}
		static_cast<void>(schemaVersion);
		economySnapshot.contestResults = {};
		if (schemaVersion == 1) {
			return true;
		}
		if (!document.contains("economy") || !document["economy"].is_object() ||
			!document.contains("cropSelection") || !document["cropSelection"].is_object()) {
			error = "Farm document is missing economy or crop-selection data.";
			return false;
		}

		const nlohmann::json& economyJson = document["economy"];
		if (!economyJson.contains("money") || !economyJson["money"].is_number_integer()) {
			error = "Farm economy money has an invalid type.";
			return false;
		}
		economySnapshot.money = economyJson["money"].get<int>();
		if (economySnapshot.money < 0 ||
			!ParseNonNegativeIntArray(
				economyJson, "cropCounts", economySnapshot.cropCounts, error, schemaVersion) ||
			!ParseNonNegativeIntArray(
				economyJson, "cropValues", economySnapshot.cropValues, error, schemaVersion) ||
			!ParseNonNegativeIntArray(
				economyJson, "seedCounts", economySnapshot.seedCounts, error, schemaVersion)) {
			if (error.empty()) {
				error = "Farm economy money is negative.";
			}
			return false;
		}
		for (std::size_t index = 0; index < economySnapshot.cropCounts.size(); ++index) {
			if ((economySnapshot.cropCounts[index] == 0) !=
				(economySnapshot.cropValues[index] == 0)) {
				error = "Farm crop count and inventory value are inconsistent.";
				return false;
			}
		}

		economySnapshot.lastHarvestQuality = {};
		if (!economyJson.contains("lastHarvestQuality")) {
			error = "Farm economy is missing last-harvest quality data.";
			return false;
		}
		const nlohmann::json& qualityJson = economyJson["lastHarvestQuality"];
		if (!qualityJson.is_null()) {
			if (!ParseQualityJson(qualityJson, schemaVersion, economySnapshot.lastHarvestQuality, error)) return false;
		}

		economySnapshot.harvestRecords = {};
		economySnapshot.harvestRecordCount = 0;
		economySnapshot.nextHarvestRecordId = 1;
		economySnapshot.contestReservationId = 0;
		economySnapshot.contestResults = {};
		if (schemaVersion >= 9) {
			if (!economyJson.contains("harvestRecords") || !economyJson["harvestRecords"].is_array() ||
				economyJson["harvestRecords"].size() > FarmEconomySystem::kMaxHarvestRecords) {
				error = "Invalid or oversized harvest inventory."; return false;
			}
			for (const auto& json : economyJson["harvestRecords"]) {
				if (!json.is_object() || !json.contains("quantity") || !json["quantity"].is_number_integer() ||
					!json.contains("quality")) { error = "Invalid harvest record fields."; return false; }
				auto& record = economySnapshot.harvestRecords[economySnapshot.harvestRecordCount++];
				if (!ReadNonNegativeInt(json["quantity"], record.quantity)) {
					error = "Harvest quantity is outside the supported range."; return false;
				}
				if (!ParseQualityJson(json["quality"], schemaVersion, record.quality, error)) return false;
				if (schemaVersion >= 12 && (!json.contains("harvestedDay") ||
					!ReadNonNegativeInt(json["harvestedDay"], record.harvestedDay))) {
					error = "Invalid harvest day."; return false;
				}
				if (schemaVersion >= 10) {
					if (!json.contains("id") || !ReadNonNegativeInt(json["id"], record.id) ||
						!json.contains("saleProtected") || !json["saleProtected"].is_boolean()) {
						error = "Invalid harvest identity or sale protection."; return false;
					}
					record.saleProtected = json["saleProtected"].get<bool>();
				} else {
					record.id = economySnapshot.nextHarvestRecordId++;
				}
			}
		}
		if (schemaVersion >= 10 && (!economyJson.contains("nextHarvestRecordId") ||
			!ReadNonNegativeInt(economyJson["nextHarvestRecordId"], economySnapshot.nextHarvestRecordId))) {
			error = "Invalid next harvest identity."; return false;
		}
		if (schemaVersion >= 11 && (!economyJson.contains("contestReservationId") ||
			!ReadNonNegativeInt(economyJson["contestReservationId"], economySnapshot.contestReservationId))) {
			error = "Invalid contest reservation identity."; return false;
		}
		if (schemaVersion >= 13) {
			if (!economyJson.contains("contestResults") || !economyJson["contestResults"].is_array() ||
				economyJson["contestResults"].size() > economySnapshot.contestResults.size()) {
				error = "Invalid contest results array."; return false;
			}
			for (const auto& json : economyJson["contestResults"]) {
				FarmEconomySystem::ContestResult result; int version = 0;
				if (!json.is_object() || !json.contains("contestDay") || !ReadNonNegativeInt(json["contestDay"], result.contestDay) ||
					(result.contestDay != 10 && result.contestDay != 20 && result.contestDay != 30) ||
					!json.contains("rulesVersion") || !ReadNonNegativeInt(json["rulesVersion"], version) || version != 1 ||
					!json.contains("recordId") || !ReadNonNegativeInt(json["recordId"], result.harvest.id) ||
					!json.contains("harvestedDay") || !ReadNonNegativeInt(json["harvestedDay"], result.harvest.harvestedDay) ||
					!json.contains("qualityPoints") || !ReadNonNegativeInt(json["qualityPoints"], result.qualityPoints) ||
					!json.contains("sizePoints") || !ReadNonNegativeInt(json["sizePoints"], result.sizePoints) ||
					!json.contains("quality") || !ParseQualityJson(json["quality"], schemaVersion, result.harvest.quality, error)) {
					error = "Invalid contest result fields."; return false;
				}
				result.harvest.quantity = 1; result.harvest.saleProtected = true;
				auto& slot = economySnapshot.contestResults[result.contestDay / 10 - 1];
				if (slot.contestDay != 0) { error = "Duplicate contest result."; return false; }
				slot = result;
			}
		}
		if (!FarmEconomySystem::ValidateSnapshot(economySnapshot)) {
			error = "Harvest records and inventory totals are inconsistent."; return false;
		}

		const nlohmann::json& selectionJson = document["cropSelection"];
		if (!selectionJson.contains("crop") || !selectionJson["crop"].is_string() ||
			!TryParseCrop(
				selectionJson["crop"].get<std::string>(), cropSelectionSnapshot.selectedCrop, schemaVersion) ||
			!farm::IsPlantableCrop(cropSelectionSnapshot.selectedCrop)) {
			error = "Farm selected crop is invalid.";
			return false;
		}
		return true;
	} catch (const std::exception& exception) {
		error = std::string("Farm persistent-state parse failed: ") + exception.what();
		return false;
	}
}

bool ReplaceWithTemporaryFile(
	const std::filesystem::path& temporaryPath,
	const std::filesystem::path& targetPath,
	std::string& error) {
	std::error_code filesystemError;
	const std::filesystem::path backupPath = targetPath.string() + ".bak";
	std::filesystem::remove(backupPath, filesystemError);
	filesystemError.clear();

	const bool hadExistingFile = std::filesystem::exists(targetPath, filesystemError);
	if (filesystemError) {
		error = "Could not inspect the existing file.";
		return false;
	}
	if (hadExistingFile) {
		std::filesystem::rename(targetPath, backupPath, filesystemError);
		if (filesystemError) {
			error = "Could not prepare the existing file for replacement.";
			return false;
		}
	}

	std::filesystem::rename(temporaryPath, targetPath, filesystemError);
	if (filesystemError) {
		if (hadExistingFile) {
			std::error_code restoreError;
			std::filesystem::rename(backupPath, targetPath, restoreError);
		}
		error = "Could not commit the new file.";
		return false;
	}
	if (hadExistingFile) {
		std::filesystem::remove(backupPath, filesystemError);
	}
	return true;
}

bool ValidateHarvestDates(const FarmDateSystem::Snapshot& date,
	const FarmEconomySystem::Snapshot& economy) {
	FarmDateSystem validator;
	if (!validator.RestoreSnapshot(date) || economy.harvestRecordCount > economy.harvestRecords.size()) return false;
	for (std::size_t i = 0; i < economy.harvestRecordCount; ++i)
		if (economy.harvestRecords[i].harvestedDay < 0 || economy.harvestRecords[i].harvestedDay > date.day) return false;
	for (const auto& result : economy.contestResults)
		if (result.contestDay > date.day) return false;
	return true;
}

bool ParseDate(const nlohmann::json& document, FarmDateSystem::Snapshot& date,
	const FarmEconomySystem::Snapshot& economy) {
	try {
		int version = 0;
		if (!TryGetSchemaVersion(document, version)) return false;
		date = {}; // Legacy records have no clock: start at Day1, keep harvest day unknown.
		if (version >= 12) {
			if (!document.contains("date") || !document["date"].is_object()) return false;
			const auto& json = document["date"];
			if (!json.contains("day") || !ReadNonNegativeInt(json["day"], date.day) ||
				!json.contains("elapsedSecondsInDay") || !json["elapsedSecondsInDay"].is_number() ||
				!json.contains("timeScale") || !json["timeScale"].is_number()) return false;
			date.elapsedSecondsInDay = json["elapsedSecondsInDay"].get<float>();
			date.timeScale = json["timeScale"].get<float>();
		}
		return ValidateHarvestDates(date, economy);
	} catch (const nlohmann::json::exception&) { return false; }
}

bool SaveJsonAtomically(
	const std::filesystem::path& targetPath,
	const nlohmann::json& json,
	std::string& error) {
	std::error_code filesystemError;
	if (!targetPath.parent_path().empty()) {
		std::filesystem::create_directories(targetPath.parent_path(), filesystemError);
		if (filesystemError) {
			error = "Could not create the save directory.";
			return false;
		}
	}
	const std::filesystem::path temporaryPath = targetPath.string() + ".tmp";
	std::filesystem::remove(temporaryPath, filesystemError);
	if (!JsonFile::Save(temporaryPath.string(), json)) {
		error = "Could not write the temporary JSON file.";
		return false;
	}
	if (!ReplaceWithTemporaryFile(temporaryPath, targetPath, error)) {
		std::filesystem::remove(temporaryPath, filesystemError);
		return false;
	}
	return true;
}
} // namespace

bool FarmDocumentSystem::Initialize(
	const std::string& directoryPath, farm::FarmGrid& grid,
	FarmEconomySystem& economySystem,
	FarmCropSelectionSystem& cropSelectionSystem, FarmDateSystem* dateSystem, FarmProgressionSystem* progressionSystem) {
	progressionSystem_ = progressionSystem;
	fallbackProgression_.Initialize();
	defaultMode_ = Progression().GetMode();
	dateSystem_ = dateSystem;
	fallbackDateSystem_.Initialize();
	directoryPath_ = directoryPath;
	saveDirectoryPath_ = (std::filesystem::path(directoryPath_) / "saves").string();
	catalogPath_ = (std::filesystem::path(directoryPath_) / "farm_documents.json").string();
	path_.clear();
	activeDocumentId_.clear();
	displayName_ = std::string(kUntitledFarmName);
	dirty_ = false;
	fileExists_ = false;
	defaultEconomySnapshot_ = economySystem.CaptureSnapshot();
	defaultCropSelectionSnapshot_ = cropSelectionSystem.CaptureSnapshot();
	SetStatus(FarmDocumentStatus::Ready, "New Farm document");

	if (!RefreshDocumentList()) {
		return false;
	}

	std::string catalogDocumentId;
	if (!LoadCatalog(catalogDocumentId)) {
		Logger::Log("FarmDocumentSystem: ignored an invalid Farm document catalog.");
	}
	if (!catalogDocumentId.empty()) {
		const auto found = std::find_if(documents_.begin(), documents_.end(), [&](const FarmDocumentEntry& entry) {
			return entry.id == catalogDocumentId;
		});
		if (found != documents_.end()) {
			return Load(found->id, grid, economySystem, cropSelectionSystem);
		}
	}
	if (!documents_.empty()) {
		return Load(documents_.front().id, grid, economySystem, cropSelectionSystem);
	}

	const std::filesystem::path legacyPath = std::filesystem::path(directoryPath_) / "farm_stage.json";
	if (JsonFile::Exists(legacyPath.string())) {
		nlohmann::json legacyDocument;
		farm::FarmGrid::Snapshot legacySnapshot;
		std::string error;
		if (JsonFile::Load(legacyPath.string(), legacyDocument) &&
			ParseSnapshot(legacyDocument, grid.GetWidth(), grid.GetHeight(), legacySnapshot, error)) {
			for (auto& tile : legacySnapshot.tiles)
				if (tile.crop == farm::CropType::TestCrop) tile.crop = farm::CropType::Carrot;
			if (grid.RestoreSnapshot(legacySnapshot)) {
				auto legacyProgression = Progression().CaptureSnapshot();
				legacyProgression.stagedUnlocks = false;
				static_cast<void>(Progression().RestoreSnapshot(legacyProgression));
				return SaveAs("Legacy Farm", grid, economySystem, cropSelectionSystem);
			}
		}
		Logger::Log("FarmDocumentSystem: legacy Farm document was not imported: " + error);
	}
	return true;
}

bool FarmDocumentSystem::Save(
	const farm::FarmGrid& grid, const FarmEconomySystem& economySystem,
	const FarmCropSelectionSystem& cropSelectionSystem) {
	if (activeDocumentId_.empty() || !fileExists_) {
		SetError("Use Save As to name this Farm document first.");
		return false;
	}
	return SaveToDocument(
		activeDocumentId_, displayName_, grid, economySystem, cropSelectionSystem);
}

bool FarmDocumentSystem::SaveAs(
	const std::string& displayName, const farm::FarmGrid& grid,
	const FarmEconomySystem& economySystem,
	const FarmCropSelectionSystem& cropSelectionSystem) {
	std::string normalizedName;
	std::string error;
	if (!NormalizeDisplayName(displayName, normalizedName, error)) {
		SetError(error);
		return false;
	}
	const bool duplicateName = std::any_of(
		documents_.begin(), documents_.end(), [&](const FarmDocumentEntry& entry) {
			return entry.displayName == normalizedName;
		});
	if (duplicateName) {
		SetError("A Farm document with this name already exists.");
		return false;
	}
	const std::string documentId = GenerateDocumentId(saveDirectoryPath_);
	if (documentId.empty()) {
		SetError("Could not allocate a unique Farm document ID.");
		return false;
	}
	return SaveToDocument(
		documentId, normalizedName, grid, economySystem, cropSelectionSystem);
}

bool FarmDocumentSystem::ArchiveCompletedSeason(const farm::FarmGrid& grid,
	const FarmEconomySystem& economySystem, const FarmCropSelectionSystem& cropSelectionSystem) {
	const auto summary = FarmContestSeasonSystem::Evaluate(Date().GetDay(), economySystem.GetContestResults());
	if (!Progression().IsContestSeason() || !summary.valid || !summary.finalized) {
		SetError("Only a completed contest season can be archived for retry.");
		return false;
	}
	const auto id = GenerateDocumentId(saveDirectoryPath_);
	if (id.empty()) { SetError("Could not allocate a season archive ID."); return false; }
	// A fresh ID preserves both the original save and this complete, possibly unsaved result.
	return SaveToDocument(id, "Season result " + id, grid, economySystem, cropSelectionSystem) && !HasError();
}

void FarmDocumentSystem::BeginUnsavedSession() {
	activeDocumentId_.clear(); path_.clear(); displayName_ = std::string(kUntitledFarmName);
	fileExists_ = false; dirty_ = true;
	// Keep the on-disk catalog on the archive until the new farm is explicitly named/saved.
	SetStatus(FarmDocumentStatus::Modified, "New unsaved farm; the previous season is archived.");
}

bool FarmDocumentSystem::Load(
	const std::string& documentId, farm::FarmGrid& grid,
	FarmEconomySystem& economySystem,
	FarmCropSelectionSystem& cropSelectionSystem) {
	if (!IsSafeDocumentId(documentId)) {
		SetError("The selected Farm document ID is invalid.");
		return false;
	}
	const auto found = std::find_if(documents_.begin(), documents_.end(), [&](const FarmDocumentEntry& entry) {
		return entry.id == documentId;
	});
	if (found == documents_.end()) {
		SetError("The selected Farm document no longer exists.");
		return false;
	}

	const std::filesystem::path documentPath =
		std::filesystem::path(saveDirectoryPath_) / (documentId + ".json");
	nlohmann::json document;
	if (!JsonFile::Load(documentPath.string(), document)) {
		SetError("Could not read the selected Farm document.");
		return false;
	}
	FarmDocumentEntry metadata;
	farm::FarmGrid::Snapshot snapshot;
	FarmEconomySystem::Snapshot economySnapshot = defaultEconomySnapshot_;
	FarmCropSelectionSystem::Snapshot cropSelectionSnapshot =
		defaultCropSelectionSnapshot_;
	std::string error;
	if (!ParseMetadata(document, documentId, metadata, error) ||
		!ParseSnapshot(document, grid.GetWidth(), grid.GetHeight(), snapshot, error) ||
		!ParsePersistentState(
			document, economySnapshot, cropSelectionSnapshot, error)) {
		SetError(error);
		return false;
	}
	FarmDateSystem::Snapshot dateSnapshot;
	if (!ParseDate(document, dateSnapshot, economySnapshot)) {
		SetError("Invalid farm date or harvest day after the saved date."); return false;
	}
	FarmProgressionMode mode = FarmProgressionMode::Trial;
	if (document["schemaVersion"].get<int>() >= 14) {
		if (!document.contains("playMode") || !document["playMode"].is_string() ||
			(document["playMode"] != "Trial" && document["playMode"] != "ContestSeason" &&
			 !(document["schemaVersion"].get<int>() >= 17 && document["playMode"] == "FreeFarming"))) {
			SetError("Invalid or missing farm play mode."); return false;
		}
		if (document["playMode"] == "ContestSeason") mode = FarmProgressionMode::ContestSeason;
		if (document["playMode"] == "FreeFarming") mode = FarmProgressionMode::FreeFarming;
	}

	bool stagedUnlocks = false;
	if (document["schemaVersion"].get<int>() >= 18) {
		if (!document.contains("stagedUnlocks") || !document["stagedUnlocks"].is_boolean()) {
			SetError("Invalid or missing stage policy."); return false;
		}
		stagedUnlocks = document["stagedUnlocks"].get<bool>();
		if (stagedUnlocks && mode != FarmProgressionMode::ContestSeason) {
			SetError("Staged unlocks require ContestSeason."); return false;
		}
	}
	bool migratedLegacyCrops = false;
	if (!MigrateLegacyCrops(snapshot, economySnapshot, cropSelectionSnapshot, migratedLegacyCrops)) {
		SetError("Legacy crop migration exceeds inventory limits or has invalid records.");
		return false;
	}
	farm::FarmGrid::Snapshot previousGridSnapshot;
	grid.CaptureSnapshot(previousGridSnapshot);
	const FarmEconomySystem::Snapshot previousEconomySnapshot =
		economySystem.CaptureSnapshot();
	const FarmCropSelectionSystem::Snapshot previousCropSelectionSnapshot =
		cropSelectionSystem.CaptureSnapshot();
	if (!grid.RestoreSnapshot(snapshot) ||
		!economySystem.RestoreSnapshot(economySnapshot) ||
		!cropSelectionSystem.RestoreSnapshot(cropSelectionSnapshot)) {
		const bool gridRollbackSucceeded =
			grid.RestoreSnapshot(previousGridSnapshot);
		const bool economyRollbackSucceeded =
			economySystem.RestoreSnapshot(previousEconomySnapshot);
		const bool cropSelectionRollbackSucceeded =
			cropSelectionSystem.RestoreSnapshot(previousCropSelectionSnapshot);
		const bool rollbackSucceeded = gridRollbackSucceeded &&
			economyRollbackSucceeded && cropSelectionRollbackSucceeded;
		if (!rollbackSucceeded) {
			Logger::Log("FarmDocumentSystem: failed to roll back a rejected document restore.");
		}
		SetError("A Farm System rejected the validated document state.");
		return false;
	}
	static_cast<void>(Date().RestoreSnapshot(dateSnapshot)); // Already validated before any state mutation.
	static_cast<void>(Progression().SetMode(mode, economySnapshot.money,
		FarmContestSeasonSystem::Evaluate(dateSnapshot.day, economySnapshot.contestResults)));
	auto loadedProgression = Progression().CaptureSnapshot();
	loadedProgression.stagedUnlocks = stagedUnlocks;
	static_cast<void>(Progression().RestoreSnapshot(loadedProgression));

	activeDocumentId_ = metadata.id;
	displayName_ = metadata.displayName;
	path_ = documentPath.string();
	fileExists_ = true;
	dirty_ = migratedLegacyCrops;
	if (!WriteCatalog()) {
		SetError("Loaded the Farm, but could not remember it for the next launch.");
		return true;
	}
	SetStatus(FarmDocumentStatus::Loaded, migratedLegacyCrops
		? "Loaded; legacy turnips converted to carrots. Save to keep the migration." : "Loaded");
	return true;
}

bool FarmDocumentSystem::Rename(
	const std::string& documentId,
	const std::string& displayName) {
	if (!IsSafeDocumentId(documentId)) {
		SetError("The selected Farm document ID is invalid.");
		return false;
	}
	std::string normalizedName;
	std::string error;
	if (!NormalizeDisplayName(displayName, normalizedName, error)) {
		SetError(error);
		return false;
	}
	const auto found = std::find_if(documents_.begin(), documents_.end(), [&](const FarmDocumentEntry& entry) {
		return entry.id == documentId;
	});
	if (found == documents_.end()) {
		SetError("The selected Farm document no longer exists.");
		return false;
	}
	const bool duplicateName = std::any_of(
		documents_.begin(), documents_.end(), [&](const FarmDocumentEntry& entry) {
			return entry.id != documentId && entry.displayName == normalizedName;
		});
	if (duplicateName) {
		SetError("A Farm document with this name already exists.");
		return false;
	}
	if (found->displayName == normalizedName) {
		SetStatus(FarmDocumentStatus::Saved, "Farm name is unchanged");
		return true;
	}

	const std::filesystem::path documentPath =
		std::filesystem::path(saveDirectoryPath_) / (documentId + ".json");
	nlohmann::json document;
	if (!JsonFile::Load(documentPath.string(), document)) {
		SetError("Could not read the selected Farm document.");
		return false;
	}
	FarmDocumentEntry metadata;
	if (!ParseMetadata(document, documentId, metadata, error)) {
		SetError(error);
		return false;
	}
	document["document"]["displayName"] = normalizedName;
	if (!SaveJsonAtomically(documentPath, document, error)) {
		SetError(error);
		return false;
	}
	if (!RefreshDocumentList()) {
		return false;
	}
	if (activeDocumentId_ == documentId) {
		displayName_ = normalizedName;
	}
	SetStatus(FarmDocumentStatus::Saved, "Renamed");
	return true;
}

bool FarmDocumentSystem::Delete(const std::string& documentId) {
	if (!IsSafeDocumentId(documentId)) {
		SetError("The selected Farm document ID is invalid.");
		return false;
	}
	const auto found = std::find_if(
		documents_.begin(), documents_.end(),
		[&](const FarmDocumentEntry& entry) { return entry.id == documentId; });
	if (found == documents_.end()) {
		SetError("The selected Farm document no longer exists.");
		return false;
	}

	const std::filesystem::path documentPath =
		std::filesystem::path(saveDirectoryPath_) / (documentId + ".json");
	std::filesystem::path stagedDeletePath = documentPath;
	stagedDeletePath += ".deleting";
	std::error_code filesystemError;
	if (!std::filesystem::is_regular_file(documentPath, filesystemError) || filesystemError) {
		SetError("The selected Farm document no longer exists.");
		return false;
	}
	std::filesystem::remove(stagedDeletePath, filesystemError);
	if (filesystemError) {
		SetError("Could not prepare the Farm document for deletion.");
		return false;
	}
	std::filesystem::rename(documentPath, stagedDeletePath, filesystemError);
	if (filesystemError) {
		SetError("Could not delete the selected Farm document.");
		return false;
	}

	const std::string previousActiveDocumentId = activeDocumentId_;
	const std::string previousDisplayName = displayName_;
	const std::string previousPath = path_;
	const std::string previousMessage = message_;
	const FarmDocumentStatus previousStatus = status_;
	const std::vector<FarmDocumentEntry> previousDocuments = documents_;
	const bool previousDirty = dirty_;
	const bool previousFileExists = fileExists_;
	const bool deletingActiveDocument = activeDocumentId_ == documentId;

	const auto rollback = [&]() {
		std::error_code rollbackError;
		std::filesystem::rename(stagedDeletePath, documentPath, rollbackError);
		if (rollbackError) {
			Logger::Log("FarmDocumentSystem: could not roll back a failed Farm delete.");
		}
		activeDocumentId_ = previousActiveDocumentId;
		displayName_ = previousDisplayName;
		path_ = previousPath;
		message_ = previousMessage;
		status_ = previousStatus;
		documents_ = previousDocuments;
		dirty_ = previousDirty;
		fileExists_ = previousFileExists;
	};

	if (deletingActiveDocument) {
		activeDocumentId_.clear();
		displayName_ = std::string(kUntitledFarmName);
		path_.clear();
		fileExists_ = false;
		dirty_ = true;
	}
	if (!RefreshDocumentList()) {
		rollback();
		SetError("Could not refresh the Farm save list after deletion.");
		return false;
	}
	if (!WriteCatalog()) {
		rollback();
		SetError("Could not update the Farm document catalog after deletion.");
		return false;
	}

	std::filesystem::remove(stagedDeletePath, filesystemError);
	if (filesystemError) {
		Logger::Log("FarmDocumentSystem: deleted Farm metadata but could not remove its staged file.");
	}
	SetStatus(
		FarmDocumentStatus::Deleted,
		deletingActiveDocument ? "Deleted; current Farm is now unsaved" : "Deleted");
	return true;
}

bool FarmDocumentSystem::Reset(
	farm::FarmGrid& grid, FarmEconomySystem& economySystem,
	FarmCropSelectionSystem& cropSelectionSystem) {
	if (grid.GetWidth() <= 0 || grid.GetHeight() <= 0) {
		SetError("Cannot reset an invalid Farm grid.");
		return false;
	}
	farm::FarmGrid::Snapshot snapshot;
	snapshot.width = grid.GetWidth();
	snapshot.height = grid.GetHeight();
	snapshot.selectedX = 0;
	snapshot.selectedY = 0;
	snapshot.tiles.assign(static_cast<std::size_t>(grid.GetTileCount()), farm::FarmTile{});
	const farm::FarmGrid::Snapshot previousGridSnapshot = [&]() {
		farm::FarmGrid::Snapshot value;
		grid.CaptureSnapshot(value);
		return value;
	}();
	const FarmEconomySystem::Snapshot previousEconomySnapshot =
		economySystem.CaptureSnapshot();
	const FarmCropSelectionSystem::Snapshot previousCropSelectionSnapshot =
		cropSelectionSystem.CaptureSnapshot();
	if (!grid.RestoreSnapshot(snapshot) ||
		!economySystem.RestoreSnapshot(defaultEconomySnapshot_) ||
		!cropSelectionSystem.RestoreSnapshot(defaultCropSelectionSnapshot_)) {
		static_cast<void>(grid.RestoreSnapshot(previousGridSnapshot));
		static_cast<void>(economySystem.RestoreSnapshot(previousEconomySnapshot));
		static_cast<void>(cropSelectionSystem.RestoreSnapshot(previousCropSelectionSnapshot));
		SetError("A Farm System rejected the reset state.");
		return false;
	}
	Date().Initialize();
	auto progressionRules = farm::FarmRules{};
	progressionRules.clearMoneyTarget = Progression().GetTargetMoney();
	Progression().Initialize(progressionRules, defaultMode_);
	static_cast<void>(Progression().SetMode(defaultMode_, economySystem.GetMoney(),
		FarmContestSeasonSystem::Evaluate(Date().GetDay(), economySystem.GetContestResults())));
	activeDocumentId_.clear();
	path_.clear();
	displayName_ = std::string(kUntitledFarmName);
	fileExists_ = false;
	dirty_ = true;
	if (!WriteCatalog()) {
		SetError("Created a new Farm, but could not update the document catalog.");
		return true;
	}
	SetStatus(FarmDocumentStatus::Reset, "New unsaved Farm");
	return true;
}

void FarmDocumentSystem::MarkDirty() noexcept {
	dirty_ = true;
	status_ = FarmDocumentStatus::Modified;
	message_ = "Unsaved changes";
}

bool FarmDocumentSystem::RefreshDocumentList() {
	documents_.clear();
	std::error_code filesystemError;
	std::filesystem::create_directories(saveDirectoryPath_, filesystemError);
	if (filesystemError) {
		SetError("Could not create or access the Farm save directory.");
		return false;
	}

	std::filesystem::directory_iterator iterator(saveDirectoryPath_, filesystemError);
	const std::filesystem::directory_iterator end;
	while (!filesystemError && iterator != end) {
		const std::filesystem::directory_entry& file = *iterator;
		if (file.is_regular_file(filesystemError) && !filesystemError &&
			file.path().extension() == ".json") {
			const std::string documentId = file.path().stem().string();
			nlohmann::json document;
			FarmDocumentEntry entry;
			std::string error;
			if (JsonFile::Load(file.path().string(), document) &&
				ParseMetadata(document, documentId, entry, error)) {
				documents_.push_back(std::move(entry));
			} else {
				Logger::Log("FarmDocumentSystem: skipped invalid save " + file.path().string() + ": " + error);
			}
		}
		iterator.increment(filesystemError);
	}
	if (filesystemError) {
		SetError("Could not enumerate the Farm save directory.");
		return false;
	}
	std::sort(documents_.begin(), documents_.end(), [](const FarmDocumentEntry& left, const FarmDocumentEntry& right) {
		if (left.savedAt != right.savedAt) {
			return left.savedAt > right.savedAt;
		}
		return left.displayName < right.displayName;
	});
	return true;
}

bool FarmDocumentSystem::WriteCatalog() {
	nlohmann::json catalog;
	catalog["schemaVersion"] = kCatalogSchemaVersion;
	catalog["activeDocumentId"] = activeDocumentId_;
	std::string error;
	if (!SaveJsonAtomically(catalogPath_, catalog, error)) {
		Logger::Log("FarmDocumentSystem: " + error);
		return false;
	}
	return true;
}

bool FarmDocumentSystem::LoadCatalog(std::string& activeDocumentId) const {
	activeDocumentId.clear();
	if (!JsonFile::Exists(catalogPath_)) {
		return true;
	}
	nlohmann::json catalog;
	if (!JsonFile::Load(catalogPath_, catalog)) {
		return false;
	}
	try {
		if (!catalog.is_object() ||
			!catalog.contains("schemaVersion") || !catalog["schemaVersion"].is_number_integer() ||
			catalog["schemaVersion"].get<int>() != kCatalogSchemaVersion ||
			!catalog.contains("activeDocumentId") || !catalog["activeDocumentId"].is_string()) {
			return false;
		}
		activeDocumentId = catalog["activeDocumentId"].get<std::string>();
		return activeDocumentId.empty() || IsSafeDocumentId(activeDocumentId);
	} catch (const std::exception&) {
		return false;
	}
}

bool FarmDocumentSystem::SaveToDocument(
	const std::string& documentId,
	const std::string& displayName,
	const farm::FarmGrid& grid,
	const FarmEconomySystem& economySystem,
	const FarmCropSelectionSystem& cropSelectionSystem) {
	if (!IsSafeDocumentId(documentId)) {
		SetError("Farm document ID is invalid.");
		return false;
	}
	farm::FarmGrid::Snapshot snapshot;
	grid.CaptureSnapshot(snapshot);
	const FarmEconomySystem::Snapshot economySnapshot =
		economySystem.CaptureSnapshot();
	const FarmCropSelectionSystem::Snapshot cropSelectionSnapshot =
		cropSelectionSystem.CaptureSnapshot();
	std::string error;
	if (!ValidateSnapshot(snapshot, grid.GetWidth(), grid.GetHeight(), error)) {
		SetError(error);
		return false;
	}

	const std::string savedAt = MakeSavedAtTimestamp();
	if (!FarmEconomySystem::ValidateSnapshot(economySnapshot)) {
		SetError("Harvest inventory is inconsistent; save rejected."); return false;
	}
	const auto dateSnapshot = Date().CaptureSnapshot();
	if (!ValidateHarvestDates(dateSnapshot, economySnapshot)) {
		SetError("Farm date and harvest days are inconsistent; save rejected."); return false;
	}
	auto document = BuildJson(snapshot, economySnapshot, cropSelectionSnapshot, documentId, displayName, savedAt);
	document["playMode"] = Progression().IsFreeFarming() ? "FreeFarming" :
		Progression().IsContestSeason() ? "ContestSeason" : "Trial";
	document["stagedUnlocks"] = Progression().StagePolicy().enabled;
	document["date"] = {{"day", dateSnapshot.day}, {"elapsedSecondsInDay", dateSnapshot.elapsedSecondsInDay},
		{"timeScale", dateSnapshot.timeScale}};
	const std::filesystem::path documentPath =
		std::filesystem::path(saveDirectoryPath_) / (documentId + ".json");
	if (!SaveJsonAtomically(
		documentPath,
		document,
		error)) {
		SetError(error);
		return false;
	}

	activeDocumentId_ = documentId;
	displayName_ = displayName;
	path_ = documentPath.string();
	fileExists_ = true;
	dirty_ = false;
	if (!RefreshDocumentList()) {
		return false;
	}
	if (!WriteCatalog()) {
		SetError("Saved the Farm, but could not remember it for the next launch.");
		return true;
	}
	SetStatus(FarmDocumentStatus::Saved, "Saved");
	return true;
}

void FarmDocumentSystem::SetStatus(FarmDocumentStatus status, std::string message) {
	status_ = status;
	message_ = std::move(message);
}

void FarmDocumentSystem::SetError(std::string message) {
	status_ = FarmDocumentStatus::Error;
	message_ = std::move(message);
	Logger::Log("FarmDocumentSystem: " + message_);
}
