#pragma once

#include "core/result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vsr {

struct MediaProbe {
    std::string path;
    int video_stream_index = -1;
    int width = 0;
    int height = 0;
    std::int64_t frame_count = 0;
    double duration_seconds = 0.0;
    std::string codec_name;
    int bit_depth = 0;
    std::vector<std::string> warnings;
};

Result<MediaProbe> probe_media(const std::string& path);

} // namespace vsr
