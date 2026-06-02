#pragma once

#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

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

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
