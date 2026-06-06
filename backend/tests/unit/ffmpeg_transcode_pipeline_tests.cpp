#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

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

TEST(FfmpegTranscodePipelineOutputDestination, rejectsExistingOutputFile) {
    const auto output_path = std::filesystem::temp_directory_path() / "vsr-existing-output.mp4";
    {
        std::ofstream output(output_path, std::ios::binary);
        output << "occupied";
    }

    const auto destination = vsr::ffmpeg_prepare_output_destination(output_path.string());

    std::filesystem::remove(output_path);
    ASSERT_FALSE(destination.ok());
    EXPECT_EQ(destination.error().code, "output_path_exists");
}

TEST(FfmpegTranscodePipelineOutputDestination, movesCompletedTempFileIntoPlace) {
    const auto output_path = std::filesystem::temp_directory_path() / "vsr-finished-output.mp4";
    std::filesystem::remove(output_path);

    const auto destination = vsr::ffmpeg_prepare_output_destination(output_path.string());

    ASSERT_TRUE(destination.ok()) << destination.error().code << ": " << destination.error().details;
    EXPECT_NE(destination.value().temp_path, destination.value().final_path);
    EXPECT_FALSE(std::filesystem::exists(destination.value().temp_path));

    {
        std::ofstream output(destination.value().temp_path, std::ios::binary);
        output << "converted";
    }

    const auto committed = vsr::ffmpeg_commit_output_destination(destination.value());

    ASSERT_TRUE(committed.ok()) << committed.error().code << ": " << committed.error().details;
    EXPECT_TRUE(std::filesystem::exists(output_path));
    EXPECT_FALSE(std::filesystem::exists(destination.value().temp_path));

    std::filesystem::remove(output_path);
}

TEST(FfmpegTranscodePipelineOutputDestination, discardsPartialTempFileOnFailure) {
    const auto output_path = std::filesystem::temp_directory_path() / "vsr-discard-output.mp4";
    std::filesystem::remove(output_path);

    const auto destination = vsr::ffmpeg_prepare_output_destination(output_path.string());

    ASSERT_TRUE(destination.ok()) << destination.error().code << ": " << destination.error().details;
    {
        std::ofstream output(destination.value().temp_path, std::ios::binary);
        output << "partial";
    }

    vsr::ffmpeg_discard_output_destination(destination.value());

    EXPECT_FALSE(std::filesystem::exists(output_path));
    EXPECT_FALSE(std::filesystem::exists(destination.value().temp_path));
}
