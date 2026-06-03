#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vsr {

struct MediaProbeRequest {
    std::string input_path;
};

struct MediaProbeSummary {
    std::string path;
    std::string name;
    std::uint64_t size_bytes = 0;
    std::string resolution;
    std::string duration;
    std::string codec;
    std::vector<std::string> warnings;
};

Result<TranscodeRequest> parse_transcode_request(const nlohmann::json& json);
Result<MediaProbeRequest> parse_media_probe_request(const nlohmann::json& json);
nlohmann::json error_to_json(const Error& error);
nlohmann::json job_snapshot_to_json(const JobSnapshot& snapshot);
nlohmann::json media_probe_summary_to_json(const MediaProbeSummary& summary);

} // namespace vsr
