#include "rtx/enhance/enhancement_backend.h"

#include <cstring>
#include <memory>

namespace rtx {

namespace {

class NullSession final : public IEnhancementSession {
 public:
  NullSession(int width, int height) : width_(width), height_(height) {}

  Status ProcessFrame(const FramePacket& input, FramePacket& output) override {
    if (input.width != width_ || input.height != height_ || output.width != width_ ||
        output.height != height_) {
      return Status::Unsupported("Null backend 只能处理尺寸不变的帧。");
    }
    if (output.size_bytes < input.size_bytes) {
      return Status::InvalidArgument("输出缓冲区过小。");
    }
    std::memcpy(output.data, input.data, input.size_bytes);
    return Status::Ok();
  }

 private:
  int width_ = 0;
  int height_ = 0;
};

class NullBackend final : public IEnhancementBackend {
 public:
  BackendInfo GetInfo() const override {
    return BackendInfo{
        .name = "null",
        .summary = "Pass-through backend for no-op export and tests.",
        .available = true,
        .supports_upscale = false,
        .supports_hdr = false,
    };
  }

  Status CreateSession(const JobConfig&,
                       const MediaInfo& input,
                       int output_width,
                       int output_height,
                       Logger&,
                       std::unique_ptr<IEnhancementSession>& session) const override {
    if (output_width != input.primary_video.width || output_height != input.primary_video.height) {
      return Status::Unsupported("Null backend 不能执行缩放。");
    }
    session = std::make_unique<NullSession>(output_width, output_height);
    return Status::Ok();
  }
};

}  // namespace

std::unique_ptr<IEnhancementBackend> CreateNullEnhancementBackend() {
  return std::make_unique<NullBackend>();
}

}  // namespace rtx

