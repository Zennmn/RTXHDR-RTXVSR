#include "api/json_dto.h"

#include <gtest/gtest.h>

using namespace vsr;

TEST(JsonDto, ParsesPlanJsonFields) {
    const auto json = nlohmann::json::parse(R"json({
      "inputPath": "C:\\Videos\\in.mp4",
      "outputPath": "C:\\Videos\\out.mp4",
      "processing": {
        "vsr": { "enabled": true, "quality": 2, "scale": 2.5 },
        "hdr": { "enabled": true, "contrast": 90, "saturation": 80, "middleGray": 48, "maxLuminance": 1200 }
      },
      "output": { "container": "mp4", "videoCodec": "hevc", "audioMode": "copy", "subtitleMode": "copy-compatible" }
    })json");

    const auto parsed = parse_transcode_request(json);

    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_EQ(parsed.value().input_path, "C:\\Videos\\in.mp4");
    EXPECT_EQ(parsed.value().output_path, "C:\\Videos\\out.mp4");
    EXPECT_TRUE(parsed.value().processing.vsr.enabled);
    EXPECT_EQ(parsed.value().processing.vsr.quality, 2);
    EXPECT_DOUBLE_EQ(parsed.value().processing.vsr.scale, 2.5);
    EXPECT_TRUE(parsed.value().processing.hdr.enabled);
    EXPECT_EQ(parsed.value().processing.hdr.contrast, 90);
    EXPECT_EQ(parsed.value().processing.hdr.saturation, 80);
    EXPECT_EQ(parsed.value().processing.hdr.middle_gray, 48);
    EXPECT_EQ(parsed.value().processing.hdr.max_luminance, 1200);
    EXPECT_EQ(parsed.value().output.container, "mp4");
    EXPECT_EQ(parsed.value().output.video_codec, "hevc");
    EXPECT_EQ(parsed.value().output.audio_mode, "copy");
    EXPECT_EQ(parsed.value().output.subtitle_mode, "copy-compatible");
}

TEST(JsonDto, DefaultsOutputFieldsAndChoosesCodecFromHdr) {
    const auto hdr_json = nlohmann::json::parse(R"json({
      "inputPath": "C:\\Videos\\in.mp4",
      "outputPath": "C:\\Videos\\out.mp4",
      "processing": { "hdr": { "enabled": true } }
    })json");
    const auto vsr_json = nlohmann::json::parse(R"json({
      "inputPath": "C:\\Videos\\in.mp4",
      "outputPath": "C:\\Videos\\out.mp4",
      "processing": { "vsr": { "enabled": true } }
    })json");

    const auto hdr = parse_transcode_request(hdr_json);
    const auto vsr = parse_transcode_request(vsr_json);

    ASSERT_TRUE(hdr.ok()) << hdr.error().message;
    EXPECT_EQ(hdr.value().output.container, "mp4");
    EXPECT_EQ(hdr.value().output.video_codec, "hevc");
    EXPECT_EQ(hdr.value().output.audio_mode, "copy");
    EXPECT_EQ(hdr.value().output.subtitle_mode, "copy-compatible");

    ASSERT_TRUE(vsr.ok()) << vsr.error().message;
    EXPECT_EQ(vsr.value().output.video_codec, "h264");
}

TEST(JsonDto, ReturnsStructuredValidationErrorForInvalidRequestJson) {
    const auto json = nlohmann::json::parse(R"json({
      "inputPath": "C:\\Videos\\in.mp4",
      "outputPath": "C:\\Videos\\out.mp4",
      "processing": { "vsr": { "enabled": true, "quality": 9 } }
    })json");

    const auto parsed = parse_transcode_request(json);

    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code, "invalid_vsr_quality");
    EXPECT_EQ(parsed.error().details, "9");
}

TEST(JsonDto, WrapsErrorsInPublicJsonEnvelope) {
    const auto json = error_to_json({"invalid_json", "Request JSON is invalid.", "line 1"});

    ASSERT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"]["code"], "invalid_json");
    EXPECT_EQ(json["error"]["message"], "Request JSON is invalid.");
    EXPECT_EQ(json["error"]["details"], "line 1");
}

TEST(JsonDto, SerializesJobSnapshotPublicFieldsOnly) {
    JobSnapshot snapshot;
    snapshot.id = "000001";
    snapshot.state = JobState::running;
    snapshot.progress.stage = JobStage::encoding;
    snapshot.progress.progress = 0.5;
    snapshot.progress.frames_done = 50;
    snapshot.progress.frames_total = 100;
    snapshot.progress.fps = 24.5;
    snapshot.progress.eta_seconds = 12;
    snapshot.input_path = "C:\\Videos\\in.mp4";
    snapshot.output_path = "C:\\Videos\\out.mp4";
    snapshot.warnings = {"using fallback"};
    snapshot.error_code = "transcode_failed";
    snapshot.error_message = "Transcode failed.";
    snapshot.error_details = "unit test";

    const auto json = job_snapshot_to_json(snapshot);

    EXPECT_EQ(json["id"], "000001");
    EXPECT_EQ(json["state"], "running");
    EXPECT_EQ(json["stage"], "encoding");
    EXPECT_EQ(json["progress"], 0.5);
    EXPECT_EQ(json["framesDone"], 50);
    EXPECT_EQ(json["framesTotal"], 100);
    EXPECT_EQ(json["fps"], 24.5);
    EXPECT_EQ(json["etaSeconds"], 12);
    EXPECT_EQ(json["inputPath"], "C:\\Videos\\in.mp4");
    EXPECT_EQ(json["outputPath"], "C:\\Videos\\out.mp4");
    EXPECT_EQ(json["warnings"][0], "using fallback");
    EXPECT_EQ(json["error"]["code"], "transcode_failed");
    EXPECT_FALSE(json.contains("request"));
}
