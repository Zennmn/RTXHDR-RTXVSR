#include "rtx/enhance/enhancement_backend.h"

#include <memory>
#include <utility>

#include "rtx/enhance/sdk_bridge_runtime.h"
#include "rtx_video_api.h"

namespace rtx {

namespace {

unsigned int MapQuality(QualityPreset preset) {
  switch (preset) {
    case QualityPreset::kFast:
      return 1;
    case QualityPreset::kBalanced:
      return 2;
    case QualityPreset::kHighQuality:
      return 4;
  }
  return 2;
}

class NvidiaCudaNgxSession final : public IEnhancementSession {
 public:
  NvidiaCudaNgxSession(JobConfig config, int input_width, int input_height, int output_width, int output_height)
      : config_(std::move(config)),
        input_width_(input_width),
        input_height_(input_height),
        output_width_(output_width),
        output_height_(output_height) {}

  ~NvidiaCudaNgxSession() override {
    try {
      rtx_video_api_cuda_shutdown();
    } catch (...) {
      // Never allow SDK bridge failures to escape a destructor.
    }
  }

  Status Initialize() {
    try {
      const API_BOOL result = rtx_video_api_cuda_create(
          nullptr, nullptr, config_.gpu_index, config_.hdr_enabled ? API_BOOL_SUCCESS : API_BOOL_FAIL,
          config_.upscale_enabled ? API_BOOL_SUCCESS : API_BOOL_FAIL);
      if (result != API_BOOL_SUCCESS) {
        return Status::Unavailable(
            "NVIDIA RTX Video backend 初始化失败。请检查 NGX、CUDA 和驱动环境。");
      }
    } catch (const SdkBridgeFatalError&) {
      return Status::Unavailable(
          "NVIDIA RTX Video backend 初始化时触发了 SDK 致命错误。");
    } catch (...) {
      return Status::Unavailable(
          "NVIDIA RTX Video backend 初始化时发生了未知异常。");
    }
    return Status::Ok();
  }

  Status ProcessFrame(const FramePacket& input, FramePacket& output) override {
    API_RECT input_rect{};
    input_rect.left = 0;
    input_rect.top = 0;
    input_rect.right = input_width_;
    input_rect.bottom = input_height_;

    API_RECT output_rect{};
    output_rect.left = 0;
    output_rect.top = 0;
    output_rect.right = output_width_;
    output_rect.bottom = output_height_;

    API_VSR_Setting vsr_settings{};
    vsr_settings.QualityLevel = MapQuality(config_.quality_preset);

    API_THDR_Setting hdr_settings{};
    hdr_settings.Contrast = config_.truehdr_contrast;
    hdr_settings.Saturation = config_.truehdr_saturation;
    hdr_settings.MiddleGray = config_.truehdr_middle_gray;
    hdr_settings.MaxLuminance = config_.truehdr_max_luminance;

    API_VSR_Setting* vsr_ptr = config_.upscale_enabled ? &vsr_settings : nullptr;
    API_THDR_Setting* hdr_ptr = config_.hdr_enabled ? &hdr_settings : nullptr;

    try {
      const API_BOOL result = rtx_video_api_cuda_evaluate_hostptr(
          input.data, output.data, input_rect, output_rect, vsr_ptr, hdr_ptr);
      if (result != API_BOOL_SUCCESS) {
        return Status::Unavailable("NVIDIA RTX Video backend 处理帧失败。");
      }
    } catch (const SdkBridgeFatalError&) {
      return Status::Unavailable(
          "NVIDIA RTX Video backend 在处理帧时触发了 SDK 致命错误。");
    } catch (...) {
      return Status::Unavailable(
          "NVIDIA RTX Video backend 在处理帧时发生了未知异常。");
    }

    return Status::Ok();
  }

 private:
  JobConfig config_;
  int input_width_ = 0;
  int input_height_ = 0;
  int output_width_ = 0;
  int output_height_ = 0;
};

class NvidiaCudaNgxBackend final : public IEnhancementBackend {
 public:
  BackendInfo GetInfo() const override {
    return BackendInfo{
        .name = "nvidia_cuda_ngx",
        .summary = "CUDA + NGX backend using the local RTX Video SDK sample API.",
        .available = true,
        .supports_upscale = true,
        .supports_hdr = true,
    };
  }

  Status CreateSession(const JobConfig& config,
                       const MediaInfo& input,
                       int output_width,
                       int output_height,
                       Logger& logger,
                       std::unique_ptr<IEnhancementSession>& session) const override {
    if (!config.upscale_enabled && !config.hdr_enabled) {
      return Status::InvalidArgument("NVIDIA 后端至少需要启用一种增强能力。");
    }

    auto created = std::make_unique<NvidiaCudaNgxSession>(
        config, input.primary_video.width, input.primary_video.height, output_width, output_height);
    auto status = created->Initialize();
    if (!status.ok()) {
      return status;
    }
    logger.Info("NVIDIA CUDA/NGX 增强会话初始化完成。");
    session = std::move(created);
    return Status::Ok();
  }
};

}  // namespace

std::unique_ptr<IEnhancementBackend> CreateNvidiaCudaNgxBackend() {
  return std::make_unique<NvidiaCudaNgxBackend>();
}

}  // namespace rtx
