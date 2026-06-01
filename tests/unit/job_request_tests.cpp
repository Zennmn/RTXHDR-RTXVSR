#include "jobs/job_types.h"

#include <gtest/gtest.h>

using namespace vsr;

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
