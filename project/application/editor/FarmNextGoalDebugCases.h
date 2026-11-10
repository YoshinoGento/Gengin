#pragma once

#include "farm/system/FarmNextGoalSystem.h"

namespace editor {
struct FarmNextGoalDebugCase {
    const char* label;
    FarmNextGoalFacts facts;
    FarmNextGoalKind expected;
};

inline constexpr FarmNextGoalDebugCase kFarmNextGoalDebugCases[] = {
    {"Goal case: New plot", {.field = FarmWaterAdvice::Till}, FarmNextGoalKind::Till},
    {"Goal case: Ready to sow", {.field = FarmWaterAdvice::Plant, .seeds = FarmGoalSeedAccess::Owned}, FarmNextGoalKind::Plant},
    {"Goal case: Missing seed", {.field = FarmWaterAdvice::Plant, .seeds = FarmGoalSeedAccess::Affordable}, FarmNextGoalKind::BuySeed},
    {"Goal case: No funds", {.field = FarmWaterAdvice::Plant}, FarmNextGoalKind::NeedFunds},
    {"Goal case: Irrigation", {.field = FarmWaterAdvice::CheckSupply}, FarmNextGoalKind::CheckWater},
    {"Goal case: Growth paused", {.field = FarmWaterAdvice::Monitor, .paused = true}, FarmNextGoalKind::ResumeGrowth},
    {"Goal case: Ready to harvest", {.field = FarmWaterAdvice::Harvest}, FarmNextGoalKind::Harvest},
    {"Goal case: Storage full", {.field = FarmWaterAdvice::Harvest, .harvestInventoryFull = true}, FarmNextGoalKind::InventoryFull},
    {"Goal case: Contest deadline", {.contest = {FarmGoalContestStatus::Today, 10}, .field = FarmWaterAdvice::Till}, FarmNextGoalKind::Submit},
    {"Goal case: Unprotected entry", {.contest = {FarmGoalContestStatus::ProtectAndReserve, 20}}, FarmNextGoalKind::ProtectAndReserve},
    {"Goal case: Entry reserved", {.contest = {FarmGoalContestStatus::Reserved, 20}}, FarmNextGoalKind::Reserved},
    {"Goal case: Pest and deadline", {.pestTile = 2, .pestWarning = true, .contest = {FarmGoalContestStatus::Today, 10}}, FarmNextGoalKind::PestWarning},
    {"Goal case: Blocked", {.blocked = true, .pestTile = 2}, FarmNextGoalKind::None},
};
}
