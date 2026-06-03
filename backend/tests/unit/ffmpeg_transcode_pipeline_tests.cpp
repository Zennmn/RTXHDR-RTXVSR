#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include <gtest/gtest.h>

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
