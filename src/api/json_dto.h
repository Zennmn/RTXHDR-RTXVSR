#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <nlohmann/json.hpp>

namespace vsr {

Result<TranscodeRequest> parse_transcode_request(const nlohmann::json& json);
nlohmann::json error_to_json(const Error& error);
nlohmann::json job_snapshot_to_json(const JobSnapshot& snapshot);

} // namespace vsr
