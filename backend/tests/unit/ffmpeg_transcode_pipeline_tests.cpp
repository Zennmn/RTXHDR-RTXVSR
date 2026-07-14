#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#if defined(VSR_ENABLE_FFMPEG)
#include "video/ffmpeg/ffmpeg_stream_utils.h"
#endif

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#if defined(VSR_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}
#endif

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

TEST(FfmpegTranscodePipelineProgress, calculatesFpsAndEtaFromFrameSamples) {
    const auto metrics = vsr::ffmpeg_progress_metrics(30, 120, 30, 2.0, 0.0);

    EXPECT_DOUBLE_EQ(metrics.fps, 15.0);
    EXPECT_EQ(metrics.eta_seconds, 6);
}

TEST(FfmpegTranscodePipelineProgress, smoothsLaterFpsSamples) {
    const auto metrics = vsr::ffmpeg_progress_metrics(60, 120, 30, 1.0, 10.0);

    EXPECT_DOUBLE_EQ(metrics.fps, 15.0);
    EXPECT_EQ(metrics.eta_seconds, 4);
}

TEST(FfmpegTranscodePipelineProgress, keepsMetricsEmptyBeforeFramesComplete) {
    const auto metrics = vsr::ffmpeg_progress_metrics(0, 120, 0, 1.0, 0.0);

    EXPECT_DOUBLE_EQ(metrics.fps, 0.0);
    EXPECT_EQ(metrics.eta_seconds, 0);
}

TEST(FfmpegTranscodePipelineOptions, choosesRequestedNvencCodecForSdrOutput) {
    vsr::OutputSettings output;
    output.video_codec = "hevc";

    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output), "hevc_nvenc");

    output.video_codec = "h264";
    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output), "h264_nvenc");
}

TEST(FfmpegTranscodePipelineOptions, choosesAv1NvencWhenRequested) {
    vsr::OutputSettings output;
    output.video_codec = "av1";

    EXPECT_STREQ(vsr::ffmpeg_nvenc_encoder_name(output), "av1_nvenc");
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

TEST(FfmpegTranscodePipelineOutput, preservesUtf8NamesAcrossTemporaryAndFinalOutputPaths) {
    const std::string utf8_directory_name = "\xE8\xBE\x93\xE5\x87\xBA-\xF0\x9F\x98\x80";
    const std::string utf8_filename = "\xE5\xBD\xB1\xE7\x89\x87-\xF0\x9F\x8E\xAC.mp4";
    const auto directory = unique_ffmpeg_test_path("utf8") / vsr::path_from_utf8(utf8_directory_name);
    const auto final_output = directory / vsr::path_from_utf8(utf8_filename);
    const auto temporary_output = vsr::ffmpeg_temporary_output_path(final_output);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);
    {
        std::ofstream file(temporary_output, std::ios::binary);
        ASSERT_TRUE(file);
        file << "encoded";
    }

    EXPECT_EQ(vsr::path_to_utf8(final_output.filename()), utf8_filename);
    EXPECT_EQ(temporary_output.parent_path(), final_output.parent_path());
    EXPECT_NE(vsr::path_to_utf8(temporary_output.filename()).find(utf8_filename), std::string::npos);

    const auto result = vsr::ffmpeg_replace_output_file(temporary_output, final_output);

    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_TRUE(std::filesystem::exists(final_output));
    EXPECT_FALSE(std::filesystem::exists(temporary_output));
    std::filesystem::remove_all(directory, ignored);
}

#if defined(VSR_ENABLE_FFMPEG)

TEST(FfmpegTranscodePipelineOutput, writesUtf8OutputPathThroughFfmpegAvio) {
    const std::string utf8_directory_name = "avio-\xE8\xBE\x93\xE5\x87\xBA-\xF0\x9F\x98\x80";
    const std::string utf8_filename = "\xE5\xBD\xB1\xE7\x89\x87-\xF0\x9F\x8E\xAC.tmp";
    const auto directory = unique_ffmpeg_test_path("avio_utf8") / vsr::path_from_utf8(utf8_directory_name);
    const auto output_path = directory / vsr::path_from_utf8(utf8_filename);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);

    AVIOContext* output = nullptr;
    const std::string ffmpeg_path = vsr::path_to_utf8(output_path);
    const int opened = avio_open(&output, ffmpeg_path.c_str(), AVIO_FLAG_WRITE);
    EXPECT_GE(opened, 0);
    if (opened >= 0) {
        const std::array<unsigned char, 4> contents = {'u', 't', 'f', '8'};
        avio_write(output, contents.data(), static_cast<int>(contents.size()));
        EXPECT_GE(avio_closep(&output), 0);
    }

    std::error_code size_error;
    EXPECT_EQ(std::filesystem::file_size(output_path, size_error), 4);
    EXPECT_FALSE(size_error);
    std::filesystem::remove_all(directory, ignored);
}

TEST(FfmpegTranscodePipelineStreams, usesMp4MuxerCompatibilityIncludingOpus) {
    const AVOutputFormat* mp4 = av_guess_format("mp4", nullptr, nullptr);

    ASSERT_NE(mp4, nullptr);
    EXPECT_TRUE(vsr::ffmpeg_muxer_supports_copy(mp4, AV_CODEC_ID_OPUS));
    EXPECT_TRUE(vsr::ffmpeg_muxer_supports_copy(mp4, AV_CODEC_ID_MOV_TEXT));
    EXPECT_FALSE(vsr::ffmpeg_muxer_supports_copy(mp4, AV_CODEC_ID_SUBRIP));
}

TEST(FfmpegTranscodePipelineStreams, copiesInputDisplayMatrixToEncodedVideoParameters) {
    const auto parameters_deleter = [](AVCodecParameters* parameters) {
        avcodec_parameters_free(&parameters);
    };
    std::unique_ptr<AVCodecParameters, decltype(parameters_deleter)> source(
        avcodec_parameters_alloc(),
        parameters_deleter);
    std::unique_ptr<AVCodecParameters, decltype(parameters_deleter)> destination(
        avcodec_parameters_alloc(),
        parameters_deleter);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(destination, nullptr);

    const std::array<std::int32_t, 9> expected_matrix = {
        0, 1 << 16, 0,
        -(1 << 16), 0, 0,
        0, 0, 1 << 30,
    };
    AVPacketSideData* source_matrix = av_packet_side_data_new(
        &source->coded_side_data,
        &source->nb_coded_side_data,
        AV_PKT_DATA_DISPLAYMATRIX,
        sizeof(expected_matrix),
        0);
    ASSERT_NE(source_matrix, nullptr);
    std::memcpy(source_matrix->data, expected_matrix.data(), sizeof(expected_matrix));

    const auto result = vsr::ffmpeg_copy_display_matrix(source.get(), destination.get());

    ASSERT_TRUE(result.ok()) << result.error().message;
    const AVPacketSideData* copied_matrix = av_packet_side_data_get(
        destination->coded_side_data,
        destination->nb_coded_side_data,
        AV_PKT_DATA_DISPLAYMATRIX);
    ASSERT_NE(copied_matrix, nullptr);
    EXPECT_EQ(copied_matrix->size, sizeof(expected_matrix));
    EXPECT_NE(copied_matrix->data, source_matrix->data);
    EXPECT_EQ(std::memcmp(copied_matrix->data, expected_matrix.data(), sizeof(expected_matrix)), 0);
}

#endif
