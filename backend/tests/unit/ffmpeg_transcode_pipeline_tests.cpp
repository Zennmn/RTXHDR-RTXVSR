#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

std::filesystem::path unique_ffmpeg_test_path(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("vsr_ffmpeg_pipeline_tests_" + std::to_string(stamp) + "_" + name);
}

} // namespace

TEST(FfmpegTranscodePipelineProgress, clampsFrameProgressToActiveEncodingRange) {
    EXPECT_DOUBLE_EQ(vsr::ffmpeg_progress_from_frames(0, 0), 0.10);
    EXPECT_DOUBLE_EQ(vsr::ffmpeg_progress_from_frames(1, 20), 0.10);
    EXPECT_DOUBLE_EQ(vsr::ffmpeg_progress_from_frames(10, 20), 0.50);
    EXPECT_DOUBLE_EQ(vsr::ffmpeg_progress_from_frames(99, 100), 0.98);
}

TEST(FfmpegTranscodePipelineOptions, choosesRequestedNvencCodecForSdrOutput) {
    vsr::OutputSettings output;
    output.video_codec = "hevc";

    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output, false), "hevc_nvenc");

    output.video_codec = "h264";
    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output, false), "h264_nvenc");
}

TEST(FfmpegTranscodePipelineOptions, forcesHevcNvencForHdrOutput) {
    vsr::OutputSettings output;
    output.video_codec = "h264";

    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output, true), "hevc_nvenc");
}

TEST(FfmpegTranscodePipelineOptions, detectsDefaultCopyModes) {
    vsr::OutputSettings output;

    EXPECT_TRUE(vsr::ffmpeg_requests_stream_copy(output));

    output.audio_mode = "none";
    output.subtitle_mode = "none";
    EXPECT_FALSE(vsr::ffmpeg_requests_stream_copy(output));
}

TEST(FfmpegTranscodePipelineOptions, scalesNvencBitrateWithOutputPixels) {
    const auto target = vsr::ffmpeg_recommended_nvenc_bitrate(
        13'940'950,
        2560,
        1380,
        5120,
        2760,
        30.0,
        true);

    EXPECT_GE(target, 65'000'000);
}

TEST(FfmpegTranscodePipelineOptions, usesPixelFloorWhenSourceBitrateIsMissing) {
    const auto target = vsr::ffmpeg_recommended_nvenc_bitrate(
        0,
        640,
        360,
        1280,
        720,
        30.0,
        false);

    EXPECT_GE(target, 2'700'000);
}

TEST(FfmpegTranscodePipelineOutput, createsTemporaryOutputBesideFinalOutput) {
    const std::filesystem::path final_output = unique_ffmpeg_test_path("output") / "movie.mp4";

    const auto temporary_output = vsr::ffmpeg_temporary_output_path(final_output);

    EXPECT_EQ(temporary_output.parent_path(), final_output.parent_path());
    EXPECT_NE(temporary_output, final_output);
    EXPECT_EQ(temporary_output.extension(), ".tmp");
    EXPECT_NE(temporary_output.filename().string().find(final_output.filename().string()), std::string::npos);
}

TEST(FfmpegTranscodePipelineOutput, rejectsMissingOutputDirectory) {
    const auto directory = unique_ffmpeg_test_path("missing_directory");
    const auto final_output = directory / "movie.mp4";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    const auto result = vsr::ffmpeg_validate_output_target(final_output);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_directory_missing");
}

TEST(FfmpegTranscodePipelineOutput, rejectsExistingFinalOutput) {
    const auto directory = unique_ffmpeg_test_path("existing_directory");
    const auto final_output = directory / "movie.mp4";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);
    {
        std::ofstream file(final_output);
        ASSERT_TRUE(file);
    }

    const auto result = vsr::ffmpeg_validate_output_target(final_output);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_file_exists");

    std::filesystem::remove_all(directory, ignored);
}

TEST(FfmpegTranscodePipelineOutput, replacesFinalOutputWithTemporaryOutput) {
    const auto directory = unique_ffmpeg_test_path("replace_directory");
    const auto final_output = directory / "movie.mp4";
    const auto temporary_output = vsr::ffmpeg_temporary_output_path(final_output);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);
    {
        std::ofstream file(temporary_output);
        ASSERT_TRUE(file);
        file << "encoded";
    }

    const auto result = vsr::ffmpeg_replace_output_file(temporary_output, final_output);

    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_TRUE(std::filesystem::exists(final_output));
    EXPECT_FALSE(std::filesystem::exists(temporary_output));
    {
        std::ifstream file(final_output);
        std::string contents;
        file >> contents;
        EXPECT_EQ(contents, "encoded");
    }

    std::filesystem::remove_all(directory, ignored);
}
