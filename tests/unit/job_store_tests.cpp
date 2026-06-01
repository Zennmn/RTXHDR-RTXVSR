#include "jobs/job_store.h"

#include <gtest/gtest.h>

using namespace vsr;

namespace {

TranscodeRequest valid_request() {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    return request;
}

} // namespace

TEST(JobStore, createsQueuedJobWithStableSnapshot) {
    JobStore store;
    const auto request = valid_request();

    const auto created = store.create(request);

    ASSERT_TRUE(created.ok()) << created.error().message;
    const auto snapshot = store.get(created.value());
    ASSERT_TRUE(snapshot.ok()) << snapshot.error().message;
    EXPECT_EQ(snapshot.value().state, JobState::queued);
    EXPECT_EQ(snapshot.value().input_path, request.input_path);
    EXPECT_EQ(snapshot.value().output_path, request.output_path);
    EXPECT_DOUBLE_EQ(snapshot.value().progress.progress, 0.0);
}

TEST(JobStore, preservesFullOriginalRequestForRunner) {
    JobStore store;
    auto request = valid_request();
    request.processing.vsr.quality = 4;
    request.processing.vsr.scale = 3.5;
    request.processing.hdr.enabled = true;
    request.processing.hdr.max_luminance = 1400;
    request.output.video_codec = "hevc";
    request.output.audio_mode = "transcode";
    request.output.subtitle_mode = "none";
    const auto id = store.create(request).value();

    const auto stored = store.get_request(id);

    ASSERT_TRUE(stored.ok()) << stored.error().message;
    EXPECT_EQ(stored.value().input_path, request.input_path);
    EXPECT_EQ(stored.value().output_path, request.output_path);
    EXPECT_EQ(stored.value().processing.vsr.quality, 4);
    EXPECT_DOUBLE_EQ(stored.value().processing.vsr.scale, 3.5);
    EXPECT_TRUE(stored.value().processing.hdr.enabled);
    EXPECT_EQ(stored.value().processing.hdr.max_luminance, 1400);
    EXPECT_EQ(stored.value().output.video_codec, "hevc");
    EXPECT_EQ(stored.value().output.audio_mode, "transcode");
    EXPECT_EQ(stored.value().output.subtitle_mode, "none");
}

TEST(JobStore, rejectsInvalidRequestWithoutStoringJob) {
    JobStore store;
    TranscodeRequest request;
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;

    const auto created = store.create(request);

    ASSERT_FALSE(created.ok());
    EXPECT_EQ(created.error().code, "input_path_required");
}

TEST(JobStore, updatesProgressAndFailure) {
    JobStore store;
    const auto id = store.create(valid_request()).value();

    JobProgress progress;
    progress.stage = JobStage::encoding;
    progress.progress = 0.5;
    progress.frames_done = 50;
    progress.frames_total = 100;
    ASSERT_TRUE(store.mark_running(id).ok());
    ASSERT_TRUE(store.update_progress(id, progress).ok());
    ASSERT_TRUE(store.mark_failed(id, {"transcode_failed", "Transcode failed.", "unit test"}).ok());

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.progress.stage, JobStage::encoding);
    EXPECT_DOUBLE_EQ(snapshot.progress.progress, 0.5);
    EXPECT_EQ(snapshot.error_code, "transcode_failed");
    EXPECT_EQ(snapshot.error_message, "Transcode failed.");
    EXPECT_EQ(snapshot.error_details, "unit test");
}

TEST(JobStore, supportsCancellationState) {
    JobStore store;
    const auto id = store.create(valid_request()).value();

    EXPECT_FALSE(store.is_cancel_requested(id));
    ASSERT_TRUE(store.request_cancel(id).ok());

    EXPECT_TRUE(store.is_cancel_requested(id));
    EXPECT_EQ(store.get(id).value().state, JobState::canceling);
}
