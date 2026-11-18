#pragma once
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmToolSystem.h"
#include <array>
#include <string>

// Bundled examples are immutable, not debug mutations of the player's current farm.
class FarmExperienceSystem final {
public:
    enum class Topic { Plant, Harvest, Soil, Trade, Water, Rain, Pests, Repellent,
        Time, Judging, Results, Development, Book, Compare, Recovery, Records,
        CampaignStart, FinalSubmission, FinalFailure, FinalMissed, Count };
    struct Entry { const char* file; FarmTool tool; bool contest; };
    static constexpr int kCount = static_cast<int>(Topic::Count);
    static constexpr int kPageSize = 4;
    static constexpr int kPages = kCount / kPageSize;
    inline static constexpr std::array<Entry, kCount> kEntries{{
        {"plant", FarmTool::Seed, false}, {"harvest", FarmTool::Harvest, false},
        {"soil", FarmTool::Hoe, false}, {"trade", FarmTool::Harvest, false},
        {"water", FarmTool::Hoe, false}, {"rain", FarmTool::Water, true},
        {"pests", FarmTool::Harvest, true}, {"repellent", FarmTool::Hoe, true},
        {"time", FarmTool::Seed, true}, {"judging", FarmTool::Harvest, true},
        {"results", FarmTool::Harvest, true}, {"development", FarmTool::Hoe, true},
        {"book", FarmTool::Harvest, false}, {"compare", FarmTool::Water, false},
        {"recovery", FarmTool::Hoe, false}, {"records", FarmTool::Seed, false},
        {"campaign_start", FarmTool::Hoe, true}, {"final_submission", FarmTool::Harvest, true},
        {"final_failure", FarmTool::Harvest, true}, {"final_missed", FarmTool::Harvest, true}
    }};
    static constexpr bool Valid(int index) noexcept { return index >= 0 && index < kCount; }
    static std::string Path(int index) {
        return Valid(index) ? std::string("Resources/farm/experience/") + kEntries[index].file + ".json" : std::string{};
    }
    static bool Load(int index, FarmDocumentSystem& documents, farm::FarmGrid& grid,
        FarmEconomySystem& economy, FarmCropSelectionSystem& selection) {
        return Valid(index) && documents.LoadPrepared(Path(index), grid, economy, selection);
    }
};
