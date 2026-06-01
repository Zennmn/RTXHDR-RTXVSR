#include "jobs/job_runner.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>

#include <memory>

using namespace vsr;

namespace {

TranscodeRequest valid_request() {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    return request;
}

class SpyPipeline final : public VideoPipeline {
public:
    Result<void> run(const TranscodeRequest& request, CancellationToken&, ProgressCallback progress) override {
        called = true;
        observed_request = request;

        JobProgress snapshot;
        snapshot.stage = JobStage::processing_rtx;
        snapshot.progress = 0.75;
        snapshot.frames_done = 75;
        snapshot.frames_total = 100;
        progress(snapshot);

        return Result<void>::Ok();
    }

    bool called = false;
    TranscodeRequest observed_request;
};

} // namespace

TEST(JobRunner, completesFakePipelineJob) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::succeeds);
    JobRunner runner(store, std::move(pipeline));

    const auto id = store.create(valid_request()).value();

    runner.run_one(id);

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::succeeded);
    EXPECT_DOUBLE_EQ(snapshot.progress.progress, 1.0);
    EXPECT_EQ(snapshot.progress.frames_total, 100);
}

TEST(JobRunner, recordsFakePipelineFailure) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::fails);
    JobRunner runner(store, std::move(pipeline));

    const auto id = store.create(valid_request()).value();

    runner.run_one(id);

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.error_code, "fake_pipeline_failed");
    EXPECT_EQ(snapshot.error_message, "Fake pipeline failed.");
}

TEST(JobRunner, passesOriginalProcessingAndOutputSettingsToPipeline) {
    JobStore store;
    auto pipeline = std::make_unique<SpyPipeline>();
    auto* spy = pipeline.get();
    JobRunner runner(store, std::move(pipeline));
    auto request = valid_request();
    request.processing.vsr.quality = 4;
    request.processing.vsr.scale = 3.0;
    request.processing.hdr.enabled = true;
    request.processing.hdr.contrast = 125;
    request.processing.hdr.saturation = 110;
    request.processing.hdr.middle_gray = 50;
    request.processing.hdr.max_luminance = 1500;
    request.output.video_codec = "hevc";
    request.output.audio_mode = "transcode";
    request.output.subtitle_mode = "none";
    const auto id = store.create(request).value();

    runner.run_one(id);

    ASSERT_TRUE(spy->called);
    EXPECT_EQ(spy->observed_request.input_path, request.input_path);
    EXPECT_EQ(spy->observed_request.output_path, request.output_path);
    EXPECT_EQ(spy->observed_request.processing.vsr.quality, 4);
    EXPECT_DOUBLE_EQ(spy->observed_request.processing.vsr.scale, 3.0);
    EXPECT_TRUE(spy->observed_request.processing.hdr.enabled);
    EXPECT_EQ(spy->observed_request.processing.hdr.contrast, 125);
    EXPECT_EQ(spy->observed_request.processing.hdr.saturation, 110);
    EXPECT_EQ(spy->observed_request.processing.hdr.middle_gray, 50);
    EXPECT_EQ(spy->observed_request.processing.hdr.max_luminance, 1500);
    EXPECT_EQ(spy->observed_request.output.video_codec, "hevc");
    EXPECT_EQ(spy->observed_request.output.audio_mode, "transcode");
    EXPECT_EQ(spy->observed_request.output.subtitle_mode, "none");
}
