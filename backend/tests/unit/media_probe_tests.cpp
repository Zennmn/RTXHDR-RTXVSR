#include "api/json_dto.h"
#include "core/utf8_path.h"
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
    EXPECT_EQ(result.value().resolution, "");
    EXPECT_EQ(result.value().duration, "");
    EXPECT_EQ(result.value().codec, "");
    ASSERT_FALSE(result.value().warnings.empty());
}

TEST(MediaProbeService, readsUtf8FileNamesWithoutUsingTheWindowsAnsiCodePage) {
    const std::string utf8_directory_name = "vsr-probe-\xE5\xAA\x92\xE4\xBD\x93-\xF0\x9F\x8E\xAC";
    const std::string utf8_filename = "\xE8\xBE\x93\xE5\x85\xA5-\xF0\x9F\x98\x80.bin";
    const auto directory = std::filesystem::temp_directory_path() / path_from_utf8(utf8_directory_name);
    const auto path = directory / path_from_utf8(utf8_filename);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);
    {
        std::ofstream output(path, std::ios::binary);
        ASSERT_TRUE(output);
        output << "utf8";
    }

    const std::string request_path = path_to_utf8(path);
    const auto result = probe_media_for_ui(request_path);

    std::filesystem::remove_all(directory, ignored);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().path, request_path);
    EXPECT_EQ(result.value().name, utf8_filename);
    EXPECT_EQ(result.value().size_bytes, 4);
}
