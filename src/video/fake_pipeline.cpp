#include "video/fake_pipeline.h"

namespace vsr {

FakePipeline::FakePipeline(FakePipelineMode mode) : mode_(mode) {}

Result<void> FakePipeline::run(const TranscodeRequest&, CancellationToken& cancellation, ProgressCallback progress) {
    for (int i = 0; i <= 4; ++i) {
        if (cancellation.requested.load()) {
            return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
        }

        JobProgress snapshot;
        snapshot.stage = i < 2 ? JobStage::decoding : JobStage::encoding;
        snapshot.progress = static_cast<double>(i) / 4.0;
        snapshot.frames_done = i * 25;
        snapshot.frames_total = 100;
        snapshot.fps = 60.0;
        snapshot.eta_seconds = 4 - i;
        progress(snapshot);
    }

    if (mode_ == FakePipelineMode::fails) {
        return Result<void>::Fail({"fake_pipeline_failed", "Fake pipeline failed.", "configured failure"});
    }

    return Result<void>::Ok();
}

} // namespace vsr
