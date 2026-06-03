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
