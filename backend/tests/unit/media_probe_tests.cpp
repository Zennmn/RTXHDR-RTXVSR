#include "api/json_dto.h"
#include "video/media_probe_service.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace vsr;

TEST(MediaProbeDto, rejectsMissingInputPath) {
    const auto parsed = parse_media_probe_request(nlohmann::json::object());

    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code, "input_path_required");
}

TEST(MediaProbeDto, parsesInputPath) {
    const auto parsed = parse_media_probe_request({{"inputPath", "C:\\Videos\\input.mp4"}});

    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_EQ(parsed.value().input_path, "C:\\Videos\\input.mp4");
}

TEST(MediaProbeDto, serializesUiMetadata) {
    MediaProbeSummary summary;
    summary.path = "C:\\Videos\\input.mp4";
    summary.name = "input.mp4";
    summary.size_bytes = 1234;
    summary.resolution = "1920x1080";
    summary.duration = "00:01:05";
    summary.codec = "h264 8-bit";
    summary.warnings = {"metadata warning"};

    const auto json = media_probe_summary_to_json(summary);

    EXPECT_EQ(json["path"], summary.path);
    EXPECT_EQ(json["name"], summary.name);
    EXPECT_EQ(json["sizeBytes"], 1234);
    EXPECT_EQ(json["resolution"], summary.resolution);
    EXPECT_EQ(json["duration"], summary.duration);
    EXPECT_EQ(json["codec"], summary.codec);
    EXPECT_EQ(json["warnings"][0], "metadata warning");
}

TEST(MediaProbeService, returnsFallbackFileMetadataWhenDetailedProbeIsUnavailable) {
    const auto path = std::filesystem::temp_directory_path() / "vsr-media-probe-fallback.bin";
    {
        std::ofstream output(path, std::ios::binary);
        output << "abcd";
    }

    const auto result = probe_media_for_ui(path.string());

    std::filesystem::remove(path);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().name, path.filename().string());
    EXPECT_EQ(result.value().size_bytes, 4);
#if !defined(VSR_ENABLE_FFMPEG)
    EXPECT_EQ(result.value().resolution, "");
    EXPECT_EQ(result.value().duration, "");
    EXPECT_EQ(result.value().codec, "");
    ASSERT_FALSE(result.value().warnings.empty());
#endif
}
