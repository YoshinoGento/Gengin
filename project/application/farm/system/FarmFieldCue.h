#pragma once
#include <bitset>
#include <cstddef>

namespace farm {
enum class FieldCue : std::size_t { Hoe, Seed, Cover, Pour, Pull, Pick, Cut, Basket, HeavyBasket,
    Bend, Seep, Ripple, BrushSoil, BrushLeaf, Equip, Count };
using FieldCues = std::bitset<static_cast<std::size_t>(FieldCue::Count)>;
inline void Mark(FieldCues& cues, FieldCue cue) noexcept { cues.set(static_cast<std::size_t>(cue)); }
}
