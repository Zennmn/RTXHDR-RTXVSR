#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace vsr {

struct CapabilitySnapshot {
    bool d3d11_available = false;
    bool rtx_sdk_found = false;
    bool vsr_available = false;
    bool truehdr_available = false;
    bool nvenc_h264_available = false;
    bool nvenc_hevc_main10_available = false;
    bool nvenc_av1_available = false;
    std::vector<std::string> messages;
};

nlohmann::json capability_snapshot_to_json(const CapabilitySnapshot& snapshot);
CapabilitySnapshot detect_capabilities();

} // namespace vsr
