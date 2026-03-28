#include "rtx/core/validation.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace rtx {

namespace {

bool IsHdrCompatibleCodec(VideoCodec codec) {
  return codec == VideoCodec::kHevc || codec == VideoCodec::kAv1;
}

}  // namespace

Status ValidateJobConfig(const JobConfig& config) {
  if (config.input_path.empty()) {
    return Status::InvalidArgument("请选择输入视频文件。");
  }
  if (config.output_path.empty()) {
    return Status::InvalidArgument("请选择输出文件路径。");
  }

  std::error_code error;
  const auto input = std::filesystem::weakly_canonical(config.input_path, error);
  error.clear();
  const auto output = std::filesystem::weakly_canonical(config.output_path, error);
  if (!error && input == output) {
    return Status::InvalidArgument("输入文件和输出文件不能是同一路径。");
  }

  if (config.hdr_enabled && !IsHdrCompatibleCodec(config.output_video_codec)) {
    return Status::InvalidArgument("HDR 导出只能使用 HEVC 或 AV1 10-bit。");
  }

  if (config.truehdr_contrast > 200) {
    return Status::InvalidArgument("TrueHDR contrast 必须在 0 到 200 之间。");
  }

  if (config.truehdr_saturation > 200) {
    return Status::InvalidArgument("TrueHDR saturation 必须在 0 到 200 之间。");
  }

  if (config.truehdr_middle_gray < 10 || config.truehdr_middle_gray > 100) {
    return Status::InvalidArgument("TrueHDR middle gray 必须在 10 到 100 之间。");
  }

  if (config.truehdr_max_luminance < 400 || config.truehdr_max_luminance > 2000) {
    return Status::InvalidArgument("TrueHDR max luminance 必须在 400 到 2000 之间。");
  }

  if (config.video_rate_control_mode == VideoRateControlMode::kTargetBitrate) {
    if (config.target_video_bitrate_kbps < 1000 || config.target_video_bitrate_kbps > 200000) {
      return Status::InvalidArgument("目标视频码率必须在 1000 到 200000 kbps 之间。");
    }
  }

  if (config.video_rate_control_mode == VideoRateControlMode::kConstantQuality) {
    if (config.constant_quality > 51) {
      return Status::InvalidArgument("Constant Quality 必须在 0 到 51 之间。");
    }
  }

  if (config.upscale_enabled && config.upscale_factor == 0) {
    return Status::InvalidArgument("超分倍数必须大于 0。");
  }

  if ((config.target_width < 0) || (config.target_height < 0)) {
    return Status::InvalidArgument("自定义分辨率不能是负数。");
  }

  if ((config.target_width > 0 && config.target_height == 0) ||
      (config.target_width == 0 && config.target_height > 0)) {
    return Status::InvalidArgument("自定义分辨率需要同时提供宽度和高度。");
  }

  if (std::filesystem::exists(config.output_path) && !config.overwrite_output) {
    return Status::InvalidArgument("输出文件已存在。若要覆盖，请显式开启 overwrite_output。");
  }

  return Status::Ok();
}

Status ResolveOutputResolution(const JobConfig& config,
                               const MediaInfo& input,
                               int& output_width,
                               int& output_height,
                               bool& decoder_scales_frame) {
  output_width = input.primary_video.width;
  output_height = input.primary_video.height;
  decoder_scales_frame = false;

  if (config.target_width > 0 && config.target_height > 0) {
    output_width = config.target_width;
    output_height = config.target_height;
  } else if (config.upscale_enabled) {
    output_width = input.primary_video.width * static_cast<int>(config.upscale_factor);
    output_height = input.primary_video.height * static_cast<int>(config.upscale_factor);
  }

  if (output_width <= 0 || output_height <= 0) {
    return Status::InvalidArgument("输出分辨率无效。");
  }

  if (config.upscale_enabled) {
    if (output_width < input.primary_video.width || output_height < input.primary_video.height) {
      return Status::InvalidArgument("启用超分时，自定义目标分辨率不能小于输入分辨率。");
    }
  } else {
    decoder_scales_frame =
        output_width != input.primary_video.width || output_height != input.primary_video.height;
  }

  return Status::Ok();
}

Status ValidateHdrRequest(const JobConfig& config, const MediaInfo& input) {
  if (!config.hdr_enabled) {
    return Status::Ok();
  }

  if (!input.has_video) {
    return Status::InvalidArgument("输入文件不包含视频流。");
  }

  if (input.primary_video.color.hdr_signaled) {
    return Status::InvalidArgument("当前输入已经被标记为 HDR，首版只支持 SDR 到 HDR。");
  }

  return Status::Ok();
}

const char* ToString(OutputContainer value) {
  switch (value) {
    case OutputContainer::kMp4:
      return "mp4";
    case OutputContainer::kMkv:
      return "mkv";
  }
  return "unknown";
}

const char* ToString(VideoCodec value) {
  switch (value) {
    case VideoCodec::kHevc:
      return "hevc";
    case VideoCodec::kAv1:
      return "av1";
    case VideoCodec::kH264:
      return "h264";
  }
  return "unknown";
}

const char* ToString(AudioMode value) {
  switch (value) {
    case AudioMode::kCopyPreferred:
      return "copy_preferred";
    case AudioMode::kAac:
      return "aac";
    case AudioMode::kDisabled:
      return "disabled";
  }
  return "unknown";
}

const char* ToString(QualityPreset value) {
  switch (value) {
    case QualityPreset::kFast:
      return "fast";
    case QualityPreset::kBalanced:
      return "balanced";
    case QualityPreset::kHighQuality:
      return "high_quality";
  }
  return "unknown";
}

const char* ToString(VideoRateControlMode value) {
  switch (value) {
    case VideoRateControlMode::kAuto:
      return "auto";
    case VideoRateControlMode::kTargetBitrate:
      return "target_bitrate";
    case VideoRateControlMode::kConstantQuality:
      return "constant_quality";
  }
  return "unknown";
}

const char* ToString(JobState value) {
  switch (value) {
    case JobState::kCreated:
      return "created";
    case JobState::kQueued:
      return "queued";
    case JobState::kAnalyzing:
      return "analyzing";
    case JobState::kPreparing:
      return "preparing";
    case JobState::kRunning:
      return "running";
    case JobState::kCompleted:
      return "completed";
    case JobState::kFailed:
      return "failed";
    case JobState::kCancelled:
      return "cancelled";
  }
  return "unknown";
}

const char* ToString(LogLevel value) {
  switch (value) {
    case LogLevel::kError:
      return "error";
    case LogLevel::kWarn:
      return "warn";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kDebug:
      return "debug";
  }
  return "unknown";
}

EventSeverity ToEventSeverity(LogLevel value) {
  switch (value) {
    case LogLevel::kError:
      return EventSeverity::kError;
    case LogLevel::kWarn:
      return EventSeverity::kWarn;
    case LogLevel::kInfo:
      return EventSeverity::kInfo;
    case LogLevel::kDebug:
      return EventSeverity::kDebug;
  }
  return EventSeverity::kInfo;
}

}  // namespace rtx
