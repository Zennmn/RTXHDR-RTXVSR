#pragma once

#include "core/utf8_path.h"
#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace vsr {

inline double ffmpeg_progress_from_frames(std::int64_t frames_done, std::int64_t frames_total) {
    if (frames_total <= 0) {
        return 0.10;
    }
    const double value = static_cast<double>(frames_done) / static_cast<double>(frames_total);
    return value < 0.10 ? 0.10 : (value > 0.98 ? 0.98 : value);
}

struct FfmpegProgressMetrics {
    double fps = 0.0;
    std::int64_t eta_seconds = 0;
};

inline FfmpegProgressMetrics ffmpeg_progress_metrics(
    std::int64_t frames_done,
    std::int64_t frames_total,
    std::int64_t frames_since_sample,
    double seconds_since_sample,
    double previous_fps) {
    double fps = previous_fps;
    if (frames_since_sample > 0 && seconds_since_sample > 0.0) {
        const double instantaneous_fps = static_cast<double>(frames_since_sample) / seconds_since_sample;
        // Smooth subsequent samples so the UI stays readable when individual RTX
        // frames have uneven processing times.
        constexpr double smoothing_factor = 0.25;
        fps = previous_fps > 0.0
            ? previous_fps + smoothing_factor * (instantaneous_fps - previous_fps)
            : instantaneous_fps;
    }

    std::int64_t eta_seconds = 0;
    if (fps > 0.0 && frames_total > frames_done) {
        eta_seconds = static_cast<std::int64_t>(std::ceil(
            static_cast<double>(frames_total - frames_done) / fps));
    }
    return {fps, eta_seconds};
}

inline const char* ffmpeg_nvenc_encoder_name(const OutputSettings& output) {
    if (output.video_codec == "av1") {
        return "av1_nvenc";
    }
    if (output.video_codec == "hevc") {
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

inline std::filesystem::path ffmpeg_temporary_output_path(const std::filesystem::path& final_output) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto filename = final_output.filename();
    filename += path_from_utf8(".vsr-" + std::to_string(stamp) + ".tmp");
    return final_output.parent_path() / filename;
}

inline Result<void> ffmpeg_validate_output_target(const std::filesystem::path& final_output) {
    const std::filesystem::path parent = final_output.parent_path();
    if (!parent.empty()) {
        std::error_code directory_error;
        const bool directory_exists = std::filesystem::exists(parent, directory_error);
        if (directory_error) {
            return Result<void>::Fail({
                "output_directory_access_failed",
                "Output directory could not be checked.",
                path_to_utf8(parent) + ": " + directory_error.message()
            });
        }
        if (!directory_exists) {
            return Result<void>::Fail({"output_directory_missing", "Output directory does not exist.", path_to_utf8(parent)});
        }
        std::error_code type_error;
        const bool is_directory = std::filesystem::is_directory(parent, type_error);
        if (type_error) {
            return Result<void>::Fail({
                "output_directory_access_failed",
                "Output directory could not be checked.",
                path_to_utf8(parent) + ": " + type_error.message()
            });
        }
        if (!is_directory) {
            return Result<void>::Fail({"output_directory_missing", "Output target parent is not a directory.", path_to_utf8(parent)});
        }
    }

    std::error_code output_error;
    const bool output_exists = std::filesystem::exists(final_output, output_error);
    if (output_error) {
        return Result<void>::Fail({
            "output_access_failed",
            "Output file could not be checked.",
            path_to_utf8(final_output) + ": " + output_error.message()
        });
    }
    if (output_exists) {
        return Result<void>::Fail({"output_file_exists", "Output file already exists.", path_to_utf8(final_output)});
    }

    return Result<void>::Ok();
}

inline Result<void> ffmpeg_replace_output_file(const std::filesystem::path& temporary_output, const std::filesystem::path& final_output) {
    const auto valid = ffmpeg_validate_output_target(final_output);
    if (!valid.ok()) {
        return Result<void>::Fail(valid.error());
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary_output, final_output, rename_error);
    if (rename_error) {
        return Result<void>::Fail({
            "output_replace_failed",
            "Temporary output could not be moved into place.",
            path_to_utf8(temporary_output) + " -> " + path_to_utf8(final_output) + ": " + rename_error.message()
        });
    }

    return Result<void>::Ok();
}

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(
        const TranscodeRequest& request,
        CancellationToken& cancellation,
        ProgressCallback progress,
        WarningCallback warning) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
