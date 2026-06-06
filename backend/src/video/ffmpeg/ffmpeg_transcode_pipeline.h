#pragma once

#include "platform/path_encoding.h"
#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <system_error>

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

struct FfmpegOutputDestination {
    std::filesystem::path final_path;
    std::filesystem::path temp_path;
    std::string final_path_utf8;
    std::string temp_path_utf8;
};

inline std::filesystem::path ffmpeg_temporary_output_path(const std::filesystem::path& final_path) {
    std::filesystem::path temp_path = final_path;
    temp_path += ".partial";
    return temp_path;
}

inline Result<FfmpegOutputDestination> ffmpeg_prepare_output_destination(const std::string& output_path) {
    FfmpegOutputDestination destination;
    destination.final_path = path_from_utf8(output_path);

    std::error_code error;
    if (std::filesystem::exists(destination.final_path, error)) {
        return Result<FfmpegOutputDestination>::Fail({
            "output_path_exists",
            "Output path already exists.",
            output_path
        });
    }
    if (error) {
        return Result<FfmpegOutputDestination>::Fail({
            "output_path_check_failed",
            "Could not inspect the output path.",
            error.message()
        });
    }

    destination.temp_path = ffmpeg_temporary_output_path(destination.final_path);
    destination.final_path_utf8 = path_to_utf8(destination.final_path);
    destination.temp_path_utf8 = path_to_utf8(destination.temp_path);
    return Result<FfmpegOutputDestination>::Ok(std::move(destination));
}

inline Result<void> ffmpeg_commit_output_destination(const FfmpegOutputDestination& destination) {
    std::error_code error;
    std::filesystem::rename(destination.temp_path, destination.final_path, error);
    if (error) {
        return Result<void>::Fail({
            "output_finalize_failed",
            "The converted file could not be moved into place.",
            error.message()
        });
    }
    return Result<void>::Ok();
}

inline Result<void> ffmpeg_discard_output_destination(const FfmpegOutputDestination& destination) {
    std::error_code error;
    const bool removed = std::filesystem::remove(destination.temp_path, error);
    if (error) {
        return Result<void>::Fail({
            "output_cleanup_failed",
            "The partial output file could not be removed.",
            error.message()
        });
    }
    (void)removed;
    return Result<void>::Ok();
}

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
