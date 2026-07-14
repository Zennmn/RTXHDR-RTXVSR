#pragma once

#include "video/video_pipeline.h"

namespace vsr {

enum class FakePipelineMode {
    succeeds,
    fails
};

class FakePipeline final : public VideoPipeline {
public:
    explicit FakePipeline(FakePipelineMode mode);
    Result<void> run(
        const TranscodeRequest& request,
        CancellationToken& cancellation,
        ProgressCallback progress,
        WarningCallback warning) override;

private:
    FakePipelineMode mode_;
};

} // namespace vsr
