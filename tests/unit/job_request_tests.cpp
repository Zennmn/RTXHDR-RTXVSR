#include "jobs/job_types.h"

#include <gtest/gtest.h>

using namespace vsr;

namespace {

template <typename T>
concept HasRequestField = requires(T value) {
    value.request;
};

static_assert(!HasRequestField<JobSnapshot>);

} // namespace

TEST(JobRequestValidation, rejectsMissingInputPath) {
    TranscodeRequest request;
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "input_path_required");
}

TEST(JobRequestValidation, rejectsRequestWithoutProcessing) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "processing_required");
}

TEST(JobRequestValidation, acceptsVsrHdrMp4Request) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    request.processing.vsr.quality = 3;
    request.processing.vsr.scale = 2.0;
    request.processing.hdr.enabled = true;
    request.output.container = "mp4";
    request.output.video_codec = "hevc";

    const auto result = validate_request(request);

    ASSERT_TRUE(result.ok()) << result.error().message;
}

TEST(JobDomainTypes, jobRecordPreservesFullRequestWhileSnapshotRemainsPathFocused) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\source.mp4";
    request.output_path = "C:\\Videos\\source.rtx.mp4";
    request.processing.vsr.enabled = true;
    request.processing.vsr.quality = 4;
    request.processing.vsr.scale = 3.5;
    request.processing.hdr.enabled = true;
    request.processing.hdr.max_luminance = 1200;
    request.output.container = "mp4";
    request.output.video_codec = "hevc";
    request.output.audio_mode = "transcode";

    JobRecord record;
    record.request = request;
    record.snapshot.id = "job-1";
    record.snapshot.state = JobState::queued;
    record.snapshot.input_path = request.input_path;
    record.snapshot.output_path = request.output_path;

    EXPECT_EQ(record.request.processing.vsr.quality, 4);
    EXPECT_DOUBLE_EQ(record.request.processing.vsr.scale, 3.5);
    EXPECT_TRUE(record.request.processing.hdr.enabled);
    EXPECT_EQ(record.request.processing.hdr.max_luminance, 1200);
    EXPECT_EQ(record.request.output.video_codec, "hevc");
    EXPECT_EQ(record.request.output.audio_mode, "transcode");

    EXPECT_EQ(record.snapshot.state, JobState::queued);
    EXPECT_EQ(record.snapshot.input_path, request.input_path);
    EXPECT_EQ(record.snapshot.output_path, request.output_path);
}
