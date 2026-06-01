#pragma once

#include "core/result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace vsr {

struct VsrSettings {
    bool enabled = false;
    int quality = 3;
    double scale = 2.0;
};

struct HdrSettings {
    bool enabled = false;
    int contrast = 100;
    int saturation = 100;
    int middle_gray = 44;
    int max_luminance = 1000;
};

struct ProcessingSettings {
    VsrSettings vsr;
    HdrSettings hdr;
};

struct OutputSettings {
    std::string container = "mp4";
    std::string video_codec = "h264";
    std::string audio_mode = "copy";
    std::string subtitle_mode = "copy-compatible";
};

struct TranscodeRequest {
    std::string input_path;
    std::string output_path;
    ProcessingSettings processing;
    OutputSettings output;
};

enum class JobState {
    queued,
    running,
    succeeded,
    failed,
    canceling,
    canceled
};

enum class JobStage {
    validating,
    probing,
    initializing_gpu,
    decoding,
    processing_rtx,
    encoding,
    muxing,
    finalizing
};

struct JobProgress {
    JobStage stage = JobStage::validating;
    double progress = 0.0;
    std::int64_t frames_done = 0;
    std::int64_t frames_total = 0;
    double fps = 0.0;
    std::int64_t eta_seconds = 0;
};

struct JobSnapshot {
    std::string id;
    JobState state = JobState::queued;
    JobProgress progress;
    std::string input_path;
    std::string output_path;
    std::vector<std::string> warnings;
    std::string error_code;
    std::string error_message;
    std::string error_details;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

struct CancellationToken {
    std::atomic_bool requested{false};
};

inline const char* to_string(JobState state) {
    switch (state) {
    case JobState::queued: return "queued";
    case JobState::running: return "running";
    case JobState::succeeded: return "succeeded";
    case JobState::failed: return "failed";
    case JobState::canceling: return "canceling";
    case JobState::canceled: return "canceled";
    }
    return "failed";
}

inline const char* to_string(JobStage stage) {
    switch (stage) {
    case JobStage::validating: return "validating";
    case JobStage::probing: return "probing";
    case JobStage::initializing_gpu: return "initializing_gpu";
    case JobStage::decoding: return "decoding";
    case JobStage::processing_rtx: return "processing_rtx";
    case JobStage::encoding: return "encoding";
    case JobStage::muxing: return "muxing";
    case JobStage::finalizing: return "finalizing";
    }
    return "validating";
}

inline Result<void> validate_request(const TranscodeRequest& request) {
    if (request.input_path.empty()) {
        return Result<void>::Fail({"input_path_required", "Input path is required.", ""});
    }
    if (request.output_path.empty()) {
        return Result<void>::Fail({"output_path_required", "Output path is required.", ""});
    }
    if (!request.processing.vsr.enabled && !request.processing.hdr.enabled) {
        return Result<void>::Fail({"processing_required", "Enable VSR, HDR, or both.", ""});
    }
    if (request.output.container != "mp4") {
        return Result<void>::Fail({"unsupported_container", "The first backend release writes MP4 output.", request.output.container});
    }
    if (request.output.video_codec != "h264" && request.output.video_codec != "hevc") {
        return Result<void>::Fail({"unsupported_video_codec", "Use h264 or hevc for MP4 output.", request.output.video_codec});
    }
    if (request.processing.vsr.enabled && (request.processing.vsr.quality < 1 || request.processing.vsr.quality > 4)) {
        return Result<void>::Fail({"invalid_vsr_quality", "VSR quality must be between 1 and 4.", std::to_string(request.processing.vsr.quality)});
    }
    if (request.processing.vsr.enabled && (request.processing.vsr.scale < 1.0 || request.processing.vsr.scale > 4.0)) {
        return Result<void>::Fail({"invalid_vsr_scale", "VSR scale must be between 1.0 and 4.0.", std::to_string(request.processing.vsr.scale)});
    }
    if (request.processing.hdr.enabled && request.output.video_codec != "hevc") {
        return Result<void>::Fail({"hdr_requires_hevc", "HDR output uses HEVC Main10 in the first backend release.", request.output.video_codec});
    }
    return Result<void>::Ok();
}

} // namespace vsr
