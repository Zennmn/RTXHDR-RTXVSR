#pragma once

#include "rtx/core/logger.h"
#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

class FfmpegProbe {
 public:
  explicit FfmpegProbe(Logger& logger);

  Status Probe(const std::string& path, MediaInfo& info) const;

  bool CanDecodeWithHardware(const std::string& codec_name) const;
  bool CanEncodeWithHardware(const std::string& codec_name) const;

 private:
  Logger& logger_;
};

}  // namespace rtx

