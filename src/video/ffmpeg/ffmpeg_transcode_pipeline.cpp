#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include "video/ffmpeg/ffmpeg_probe.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace vsr {

FfmpegTranscodePipeline::FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx) : rtx_(std::move(rtx)) {}

Result<void> FfmpegTranscodePipeline::run(
    const TranscodeRequest& request,
    CancellationToken& cancellation,
    ProgressCallback progress) {
    std::error_code input_status_error;
    const bool input_exists = std::filesystem::exists(request.input_path, input_status_error);
    if (input_status_error) {
        return Result<void>::Fail({
            "input_access_failed",
            "Input file could not be checked.",
            request.input_path + ": " + input_status_error.message()
        });
    }
    if (!input_exists) {
        return Result<void>::Fail({"input_not_found", "Input file does not exist.", request.input_path});
    }

    JobProgress validating;
    validating.stage = JobStage::validating;
    validating.progress = 0.01;
    progress(validating);

    const auto probe = probe_media(request.input_path);
    if (!probe.ok()) {
        return Result<void>::Fail(probe.error());
    }

    JobProgress probing;
    probing.stage = JobStage::probing;
    probing.progress = 0.05;
    probing.frames_total = probe.value().frame_count;
    progress(probing);

    if (cancellation.requested.load()) {
        return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
    }

    return Result<void>::Fail(
        {"hardware_pipeline_not_connected",
         "FFmpeg probing is connected; the D3D11 decode/process/encode loop is not connected in this task.",
         "Continue with Task 8 to connect the hardware frame loop."});
}

} // namespace vsr
