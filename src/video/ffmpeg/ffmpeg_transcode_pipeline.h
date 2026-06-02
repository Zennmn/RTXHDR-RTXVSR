#pragma once

#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

#include <memory>

namespace vsr {

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
