#pragma once

#include "core/result.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace vsr {

// CQ numbers are codec-specific, not a cross-codec perceptual quality scale.
inline double nvenc_target_quality(std::string_view codec) {
    return codec == "av1" ? 24.0 : 18.0;
}

struct Av1LevelSetting {
    int index;
    int tier;
};

// AOM AV1 specification, Annex A. Use defined levels only. In particular,
// NVENC's automatic level may emit 7.3 (index 23), rejected by libaom.
// Start at 4.0 to accommodate the encoder's automatic tile layout. NVENC
// enforces the selected level's remaining coding/tile constraints at init.
inline Result<Av1LevelSetting> select_av1_level(
    int width, int height, double fps, std::int64_t max_rate, std::int64_t buffer_bits) {
    if (width <= 0 || height <= 0 || !std::isfinite(fps) || fps <= 0.0 ||
        max_rate <= 0 || buffer_bits <= 0) {
        return Result<Av1LevelSetting>::Fail({
            "invalid_av1_level_input", "AV1 level selection requires valid dimensions, frame rate and VBV limits.", ""});
    }
    struct Limit {
        int index;
        std::int64_t picture;
        int width;
        int height;
        std::int64_t display_rate;
        std::int64_t main_rate;
        std::int64_t high_rate;
    };
    static constexpr Limit limits[] = {
        {8,  2'359'296,  6144, 3456,    70'778'880,  12'000'000,  30'000'000},
        {9,  2'359'296,  6144, 3456,   141'557'760,  20'000'000,  50'000'000},
        {12, 8'912'896,  8192, 4352,   267'386'880,  30'000'000, 100'000'000},
        {13, 8'912'896,  8192, 4352,   534'773'760,  40'000'000, 160'000'000},
        {14, 8'912'896,  8192, 4352, 1'069'547'520,  60'000'000, 240'000'000},
        {16,35'651'584, 16384, 8704, 1'069'547'520,  60'000'000, 240'000'000},
        {17,35'651'584, 16384, 8704, 2'139'095'040, 100'000'000, 480'000'000},
        {18,35'651'584, 16384, 8704, 4'278'190'080, 160'000'000, 800'000'000},
    };
    const auto picture = static_cast<std::int64_t>(width) * height;
    const double display_rate = static_cast<double>(picture) * fps;
    // Annex A limits the buffer to one second of the level's bitrate.
    const auto required_rate = std::max(max_rate, buffer_bits);
    for (const auto& limit : limits) {
        if (picture > limit.picture || width > limit.width || height > limit.height ||
            display_rate > static_cast<double>(limit.display_rate) || fps > 300.0) {
            continue;
        }
        if (required_rate <= limit.main_rate) {
            return Result<Av1LevelSetting>::Ok({limit.index, 0});
        }
        if (required_rate <= limit.high_rate) {
            return Result<Av1LevelSetting>::Ok({limit.index, 1});
        }
    }
    return Result<Av1LevelSetting>::Fail({
        "unsupported_av1_level", "Output exceeds the supported AV1 level limits (up to 6.2).",
        "Reduce the output resolution, frame rate or bitrate."});
}

} // namespace vsr
