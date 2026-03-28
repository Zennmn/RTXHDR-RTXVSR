#pragma once

#include <memory>
#include <string>

#include "rtx/core/logger.h"
#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

struct BackendInfo {
  std::string name;
  std::string summary;
  bool available = false;
  bool supports_upscale = false;
  bool supports_hdr = false;
};

class IEnhancementSession {
 public:
  virtual ~IEnhancementSession() = default;
  virtual Status ProcessFrame(const FramePacket& input, FramePacket& output) = 0;
};

class IEnhancementBackend {
 public:
  virtual ~IEnhancementBackend() = default;

  virtual BackendInfo GetInfo() const = 0;
  virtual Status CreateSession(const JobConfig& config,
                               const MediaInfo& input,
                               int output_width,
                               int output_height,
                               Logger& logger,
                               std::unique_ptr<IEnhancementSession>& session) const = 0;
};

std::unique_ptr<IEnhancementBackend> CreateNullEnhancementBackend();
std::unique_ptr<IEnhancementBackend> CreateNvidiaCudaNgxBackend();

}  // namespace rtx

