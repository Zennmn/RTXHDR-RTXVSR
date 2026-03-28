#pragma once

#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

Status ValidateJobConfig(const JobConfig& config);
Status ResolveOutputResolution(const JobConfig& config,
                               const MediaInfo& input,
                               int& output_width,
                               int& output_height,
                               bool& decoder_scales_frame);
Status ValidateHdrRequest(const JobConfig& config, const MediaInfo& input);

}  // namespace rtx

