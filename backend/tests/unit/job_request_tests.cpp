#include "jobs/job_types.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace vsr;

namespace {

template <typename T>
concept HasRequestField = requires(T value) {
    value.request;
};

static_assert(!HasRequestField<JobSnapshot>);

std::string from_u8(std::u8string_view value) {
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

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

TEST(JobRequestValidation, rejectsSameInputAndOutputPath) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\in.mp4";
    request.processing.vsr.enabled = true;

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_path_matches_input_path");
}

TEST(JobRequestValidation, rejectsCaseInsensitiveEquivalentInputAndOutputPath) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\Input.mp4";
    request.output_path = "c:\\videos\\.\\INPUT.mp4";
    request.processing.vsr.enabled = true;

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_path_matches_input_path");
}

TEST(JobRequestValidation, rejectsUtf8EquivalentInputAndOutputPath) {
    TranscodeRequest request;
    request.input_path = from_u8(u8"C:\\Videos\\\u6D4B\u8BD5.mp4");
    request.output_path = from_u8(u8"c:/videos/./\u6D4B\u8BD5.mp4");
    request.processing.vsr.enabled = true;

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_path_matches_input_path");
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

TEST(JobRequestValidation, acceptsExplicitlyDroppingAudioAndSubtitles) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    request.output.audio_mode = "none";
    request.output.subtitle_mode = "none";

    const auto result = validate_request(request);

    ASSERT_TRUE(result.ok()) << result.error().message;
}

TEST(JobRequestValidation, rejectsUnsupportedAudioMode) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    request.output.audio_mode = "transcode";

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "unsupported_audio_mode");
    EXPECT_EQ(result.error().details, "transcode");
}

TEST(JobRequestValidation, rejectsUnsupportedSubtitleMode) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    request.output.subtitle_mode = "copy-all";

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "unsupported_subtitle_mode");
    EXPECT_EQ(result.error().details, "copy-all");
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
    request.output.audio_mode = "none";

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
    EXPECT_EQ(record.request.output.audio_mode, "none");

    EXPECT_EQ(record.snapshot.state, JobState::queued);
    EXPECT_EQ(record.snapshot.input_path, request.input_path);
    EXPECT_EQ(record.snapshot.output_path, request.output_path);
}
