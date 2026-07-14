#pragma once

#include "core/result.h"

struct AVCodecParameters;
struct AVOutputFormat;

namespace vsr {

// Keep FFmpeg types out of the public pipeline header. These helpers are
// exposed only so FFmpeg-enabled tests can exercise the muxer policy and the
// global display-matrix transfer without requiring an RTX device.
bool ffmpeg_muxer_supports_copy(const AVOutputFormat* format, int codec_id);
Result<void> ffmpeg_copy_display_matrix(
    const AVCodecParameters* source,
    AVCodecParameters* destination);

} // namespace vsr
