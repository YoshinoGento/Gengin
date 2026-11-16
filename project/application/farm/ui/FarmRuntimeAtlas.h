#pragma once
#include "farm/ui/FarmRuntimeLabels.h"
#include <array>
#include <istream>

namespace farmui {
// Detect accidental asset/build mixing before creating UI resources; not an authenticity check.
inline bool MatchesRuntimeAtlas(std::istream& input) {
    std::array<char, 4096> buffer{};
    std::uint64_t bytes = 0;
    std::uint64_t fingerprint = 14695981039346656037ULL;
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > kAtlasByteCount - bytes) return false;
        bytes += count;
        for (std::size_t i = 0; i < count; ++i) {
            fingerprint ^= static_cast<unsigned char>(buffer[i]);
            fingerprint *= 1099511628211ULL;
        }
    }
    return input.eof() && !input.bad() && bytes == kAtlasByteCount &&
        fingerprint == kAtlasFingerprint;
}
}
