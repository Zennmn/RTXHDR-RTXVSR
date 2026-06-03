#include "api/json_dto.h"

#include <exception>

namespace vsr {

Result<TranscodeRequest> parse_transcode_request(const nlohmann::json& json) {
    try {
        TranscodeRequest request;
        request.input_path = json.value("inputPath", "");
        request.output_path = json.value("outputPath", "");

        const auto processing = json.value("processing", nlohmann::json::object());
        const auto vsr = processing.value("vsr", nlohmann::json::object());
        request.processing.vsr.enabled = vsr.value("enabled", false);
        request.processing.vsr.quality = vsr.value("quality", 3);
        request.processing.vsr.scale = vsr.value("scale", 2.0);

        const auto hdr = processing.value("hdr", nlohmann::json::object());
        request.processing.hdr.enabled = hdr.value("enabled", false);
        request.processing.hdr.contrast = hdr.value("contrast", 100);
        request.processing.hdr.saturation = hdr.value("saturation", 100);
        request.processing.hdr.middle_gray = hdr.value("middleGray", 44);
        request.processing.hdr.max_luminance = hdr.value("maxLuminance", 1000);

        const auto output = json.value("output", nlohmann::json::object());
        request.output.container = output.value("container", "mp4");
        request.output.video_codec = output.value("videoCodec", request.processing.hdr.enabled ? "hevc" : "h264");
        request.output.audio_mode = output.value("audioMode", "copy");
        request.output.subtitle_mode = output.value("subtitleMode", "copy-compatible");

        const auto valid = validate_request(request);
        if (!valid.ok()) {
            return Result<TranscodeRequest>::Fail(valid.error());
        }
        return Result<TranscodeRequest>::Ok(std::move(request));
    } catch (const std::exception& ex) {
        return Result<TranscodeRequest>::Fail({"invalid_json", "Request JSON is invalid.", ex.what()});
    }
}

Result<MediaProbeRequest> parse_media_probe_request(const nlohmann::json& json) {
    try {
        MediaProbeRequest request;
        request.input_path = json.value("inputPath", "");
        if (request.input_path.empty()) {
            return Result<MediaProbeRequest>::Fail({"input_path_required", "Input path is required.", ""});
        }
        return Result<MediaProbeRequest>::Ok(std::move(request));
    } catch (const std::exception& ex) {
        return Result<MediaProbeRequest>::Fail({"invalid_json", "Request JSON is invalid.", ex.what()});
    }
}

nlohmann::json error_to_json(const Error& error) {
    return {
        {"error", {
            {"code", error.code},
            {"message", error.message},
            {"details", error.details},
        }},
    };
}

nlohmann::json job_snapshot_to_json(const JobSnapshot& snapshot) {
    nlohmann::json json{
        {"id", snapshot.id},
        {"state", to_string(snapshot.state)},
        {"stage", to_string(snapshot.progress.stage)},
        {"progress", snapshot.progress.progress},
        {"framesDone", snapshot.progress.frames_done},
        {"framesTotal", snapshot.progress.frames_total},
        {"fps", snapshot.progress.fps},
        {"etaSeconds", snapshot.progress.eta_seconds},
        {"inputPath", snapshot.input_path},
        {"outputPath", snapshot.output_path},
        {"warnings", snapshot.warnings},
    };

    if (!snapshot.error_code.empty()) {
        json["error"] = {
            {"code", snapshot.error_code},
            {"message", snapshot.error_message},
            {"details", snapshot.error_details},
        };
    }

    return json;
}

nlohmann::json media_probe_summary_to_json(const MediaProbeSummary& summary) {
    return {
        {"path", summary.path},
        {"name", summary.name},
        {"sizeBytes", summary.size_bytes},
        {"resolution", summary.resolution},
        {"duration", summary.duration},
        {"codec", summary.codec},
        {"warnings", summary.warnings},
    };
}

} // namespace vsr
