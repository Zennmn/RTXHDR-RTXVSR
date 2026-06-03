#pragma once

#include "api/json_dto.h"
#include "core/result.h"

#include <string>

namespace vsr {

std::string format_duration_seconds(double duration_seconds);
Result<MediaProbeSummary> probe_media_for_ui(const std::string& path);

} // namespace vsr
