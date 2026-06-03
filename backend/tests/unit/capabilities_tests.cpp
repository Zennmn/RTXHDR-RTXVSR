#include "platform/capabilities.h"

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace vsr;

TEST(Capabilities, SerializesAllPublicKeys) {
    CapabilitySnapshot snapshot;
    snapshot.d3d11_available = false;
    snapshot.rtx_sdk_found = false;
    snapshot.vsr_available = false;
    snapshot.truehdr_available = false;
    snapshot.nvenc_h264_available = false;
    snapshot.nvenc_hevc_main10_available = false;

    const auto json = capability_snapshot_to_json(snapshot);
    const std::set<std::string> keys = {
        "d3d11Available",
        "rtxSdkFound",
        "vsrAvailable",
        "truehdrAvailable",
        "nvencH264Available",
        "nvencHevcMain10Available",
        "messages",
    };

    EXPECT_EQ(json.size(), keys.size());
    EXPECT_TRUE(json.contains("d3d11Available"));
    EXPECT_TRUE(json.contains("rtxSdkFound"));
    EXPECT_TRUE(json.contains("vsrAvailable"));
    EXPECT_TRUE(json.contains("truehdrAvailable"));
    EXPECT_TRUE(json.contains("nvencH264Available"));
    EXPECT_TRUE(json.contains("nvencHevcMain10Available"));
    EXPECT_TRUE(json.contains("messages"));
    for (const auto& item : json.items()) {
        EXPECT_TRUE(keys.contains(item.key())) << item.key();
    }
}

TEST(Capabilities, SerializesUnavailableHardwareSnapshot) {
    CapabilitySnapshot snapshot;
    snapshot.messages = {"D3D11 is unavailable.", "nvngx_vsr.dll was not found."};

    const auto json = capability_snapshot_to_json(snapshot);

    EXPECT_FALSE(json["d3d11Available"]);
    EXPECT_FALSE(json["rtxSdkFound"]);
    EXPECT_FALSE(json["vsrAvailable"]);
    EXPECT_FALSE(json["truehdrAvailable"]);
    EXPECT_FALSE(json["nvencH264Available"]);
    EXPECT_FALSE(json["nvencHevcMain10Available"]);
}

TEST(Capabilities, PreservesMessages) {
    CapabilitySnapshot snapshot;
    snapshot.messages = {"first message", "second message"};

    const auto json = capability_snapshot_to_json(snapshot);

    ASSERT_EQ(json["messages"].size(), 2);
    EXPECT_EQ(json["messages"][0], "first message");
    EXPECT_EQ(json["messages"][1], "second message");
}
