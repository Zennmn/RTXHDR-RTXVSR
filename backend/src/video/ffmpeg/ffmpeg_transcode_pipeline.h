#pragma once

#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace vsr {

inline double ffmpeg_progress_from_frames(std::int64_t frames_done, std::int64_t frames_total) {
    if (frames_total <= 0) {
        return 0.10;
    }
    const double value = static_cast<double>(frames_done) / static_cast<double>(frames_total);
    return value < 0.10 ? 0.10 : (value > 0.98 ? 0.98 : value);
}

inline const char* ffmpeg_nvenc_encoder_name(const OutputSettings& output, bool hdr_enabled) {
    if (hdr_enabled || output.video_codec == "hevc") {
        return "hevc_nvenc";
    }
    return "h264_nvenc";
}

inline bool ffmpeg_requests_stream_copy(const OutputSettings& output) {
    return output.audio_mode == "copy" || output.subtitle_mode == "copy-compatible";
}

inline std::int64_t ffmpeg_recommended_nvenc_bitrate(
    std::int64_t source_bit_rate,
    int input_width,
    int input_height,
    int output_width,
    int output_height,
    double frames_per_second,
    bool hdr_enabled) {
    constexpr double minimum_bit_rate = 2'500'000.0;
    constexpr double maximum_bit_rate = 160'000'000.0;

    const double output_pixels = static_cast<double>(std::max(output_width, 0)) * static_cast<double>(std::max(output_height, 0));
    if (output_pixels <= 0.0) {
        return static_cast<std::int64_t>(minimum_bit_rate);
    }

    const double input_pixels = static_cast<double>(std::max(input_width, 0)) * static_cast<double>(std::max(input_height, 0));
    const double source_based = source_bit_rate > 0 && input_pixels > 0.0
        ? static_cast<double>(source_bit_rate) * (output_pixels / input_pixels) * (hdr_enabled ? 1.25 : 1.0)
        : 0.0;

    const double safe_fps = frames_per_second > 1.0 ? frames_per_second : 30.0;
    const double bits_per_pixel_frame = hdr_enabled ? 0.12 : 0.10;
    const double pixel_based = output_pixels * safe_fps * bits_per_pixel_frame;

    return static_cast<std::int64_t>(std::llround(std::clamp(std::max(source_based, pixel_based), minimum_bit_rate, maximum_bit_rate)));
}

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
