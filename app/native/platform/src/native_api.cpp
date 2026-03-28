#include "rtx/platform/native_api.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rtx/core/logger.h"
#include "rtx/core/status.h"
#include "rtx/core/types.h"
#include "rtx/pipeline/job_manager.h"
#include "rtx/platform/system_probe.h"
#include "rtx/video_io/ffmpeg_probe.h"
#include "rtx/video_io/ffmpeg_runtime.h"

namespace {

struct LibraryState {
  std::mutex mutex;
  bool initialized = false;
  std::string last_error;
  rtx::RuntimePaths runtime_paths;
  rtx::SystemProbe system_probe;
  std::unique_ptr<rtx::JobManager> job_manager;
};

LibraryState& GetState() {
  static LibraryState state;
  return state;
}

std::uint32_t ToApiStatus(const rtx::Status& status) {
  switch (status.code()) {
    case rtx::StatusCode::kOk:
      return RTX_STATUS_OK;
    case rtx::StatusCode::kInvalidArgument:
      return RTX_STATUS_INVALID_ARGUMENT;
    case rtx::StatusCode::kNotFound:
      return RTX_STATUS_NOT_FOUND;
    case rtx::StatusCode::kUnavailable:
      return RTX_STATUS_UNAVAILABLE;
    case rtx::StatusCode::kDependencyMissing:
      return RTX_STATUS_DEPENDENCY_MISSING;
    case rtx::StatusCode::kUnsupported:
      return RTX_STATUS_UNSUPPORTED;
    case rtx::StatusCode::kIoError:
      return RTX_STATUS_IO_ERROR;
    case rtx::StatusCode::kInternal:
      return RTX_STATUS_INTERNAL;
    case rtx::StatusCode::kCancelled:
      return RTX_STATUS_CANCELLED;
  }
  return RTX_STATUS_INTERNAL;
}

void SetLastError(const std::string& message) {
  auto& state = GetState();
  std::scoped_lock lock(state.mutex);
  state.last_error = message;
}

template <std::size_t N>
void CopyUtf8(const std::string& value, char (&buffer)[N]) {
  std::memset(buffer, 0, N);
  if (N == 0) {
    return;
  }
  const auto count = std::min<std::size_t>(N - 1, value.size());
  std::memcpy(buffer, value.data(), count);
}

rtx::JobConfig ConvertConfig(const RtxJobConfigV1& source) {
  rtx::JobConfig config;
  config.input_path = source.input_path != nullptr ? source.input_path : "";
  config.output_path = source.output_path != nullptr ? source.output_path : "";
  config.output_container = source.output_container == RTX_CONTAINER_MP4
                                ? rtx::OutputContainer::kMp4
                                : rtx::OutputContainer::kMkv;
  config.output_video_codec = source.output_video_codec == RTX_CODEC_AV1
                                  ? rtx::VideoCodec::kAv1
                                  : (source.output_video_codec == RTX_CODEC_H264
                                         ? rtx::VideoCodec::kH264
                                         : rtx::VideoCodec::kHevc);
  config.output_audio_mode = source.output_audio_mode == RTX_AUDIO_AAC
                                 ? rtx::AudioMode::kAac
                                 : (source.output_audio_mode == RTX_AUDIO_DISABLED
                                        ? rtx::AudioMode::kDisabled
                                        : rtx::AudioMode::kCopyPreferred);
  config.quality_preset = source.quality_preset == RTX_QUALITY_FAST
                              ? rtx::QualityPreset::kFast
                              : (source.quality_preset == RTX_QUALITY_HIGH
                                     ? rtx::QualityPreset::kHighQuality
                                     : rtx::QualityPreset::kBalanced);
  config.target_width = source.target_width;
  config.target_height = source.target_height;
  config.upscale_enabled = source.upscale_enabled != 0;
  config.upscale_factor = source.upscale_factor;
  config.hdr_enabled = source.hdr_enabled != 0;
  config.truehdr_contrast = source.truehdr_contrast;
  config.truehdr_saturation = source.truehdr_saturation;
  config.truehdr_middle_gray = source.truehdr_middle_gray;
  config.truehdr_max_luminance = source.truehdr_max_luminance;
  config.video_rate_control_mode =
      source.video_rate_control_mode == RTX_RATE_CONTROL_TARGET_BITRATE
          ? rtx::VideoRateControlMode::kTargetBitrate
          : (source.video_rate_control_mode == RTX_RATE_CONTROL_CONSTANT_QUALITY
                 ? rtx::VideoRateControlMode::kConstantQuality
                 : rtx::VideoRateControlMode::kAuto);
  config.target_video_bitrate_kbps = source.target_video_bitrate_kbps;
  config.constant_quality = source.constant_quality;
  config.gpu_index = source.gpu_index;
  config.keep_audio = source.keep_audio != 0;
  config.overwrite_output = source.overwrite_output != 0;
  config.lock_aspect_ratio = source.lock_aspect_ratio != 0;
  config.logging_level = source.logging_level == RTX_LOG_ERROR
                             ? rtx::LogLevel::kError
                             : (source.logging_level == RTX_LOG_WARN
                                    ? rtx::LogLevel::kWarn
                                    : (source.logging_level == RTX_LOG_DEBUG ? rtx::LogLevel::kDebug
                                                                              : rtx::LogLevel::kInfo));
  return config;
}

void FillSystemProbe(const rtx::SystemProbe& source, RtxSystemProbeV1& target) {
  target.struct_size = sizeof(RtxSystemProbeV1);
  CopyUtf8(source.os_version, target.os_version);
  CopyUtf8(source.gpu_name, target.gpu_name);
  CopyUtf8(source.driver_version, target.driver_version);
  CopyUtf8(source.cuda_version, target.cuda_version);
  target.has_nvidia_gpu = source.has_nvidia_gpu ? 1 : 0;
  target.ngx_vsr_available = source.ngx_vsr_available ? 1 : 0;
  target.ngx_truehdr_available = source.ngx_truehdr_available ? 1 : 0;
  target.hdr_export_available = source.hdr_export_available ? 1 : 0;
  target.h264_hw_decode = source.h264_hw_decode ? 1 : 0;
  target.hevc_hw_decode = source.hevc_hw_decode ? 1 : 0;
  target.av1_hw_decode = source.av1_hw_decode ? 1 : 0;
  target.h264_hw_encode = source.h264_hw_encode ? 1 : 0;
  target.hevc_hw_encode = source.hevc_hw_encode ? 1 : 0;
  target.av1_hw_encode = source.av1_hw_encode ? 1 : 0;
  CopyUtf8(source.supported_pixel_formats, target.supported_pixel_formats);
  CopyUtf8(source.notes, target.notes);
}

void FillMediaInfo(const rtx::MediaInfo& source, RtxMediaInfoV1& target) {
  target.struct_size = sizeof(RtxMediaInfoV1);
  CopyUtf8(source.container_name, target.container_name);
  CopyUtf8(source.primary_video.codec_name, target.video_codec);
  CopyUtf8(source.primary_audio.codec_name, target.audio_codec);
  CopyUtf8(source.primary_video.color.pixel_format, target.pixel_format);
  CopyUtf8(source.primary_video.color.primaries, target.color_primaries);
  CopyUtf8(source.primary_video.color.transfer, target.transfer);
  CopyUtf8(source.primary_video.color.matrix, target.matrix);
  CopyUtf8(source.primary_video.color.range, target.color_range);
  target.width = source.primary_video.width;
  target.height = source.primary_video.height;
  target.frame_rate = source.primary_video.frame_rate;
  target.duration_seconds = source.duration_seconds;
  target.frame_count =
      static_cast<std::uint64_t>(std::max<std::int64_t>(0, source.primary_video.frame_count));
  target.bit_depth = source.primary_video.color.bit_depth;
  target.audio_channels = source.primary_audio.channels;
  target.audio_sample_rate = source.primary_audio.sample_rate;
  target.has_audio = source.has_audio ? 1 : 0;
  target.hdr_signaled = source.primary_video.color.hdr_signaled ? 1 : 0;
}

void FillProgress(const rtx::JobProgress& source, RtxJobProgressV1& target) {
  target.struct_size = sizeof(RtxJobProgressV1);
  switch (source.state) {
    case rtx::JobState::kCreated:
      target.state = RTX_JOB_CREATED;
      break;
    case rtx::JobState::kQueued:
      target.state = RTX_JOB_QUEUED;
      break;
    case rtx::JobState::kAnalyzing:
      target.state = RTX_JOB_ANALYZING;
      break;
    case rtx::JobState::kPreparing:
      target.state = RTX_JOB_PREPARING;
      break;
    case rtx::JobState::kRunning:
      target.state = RTX_JOB_RUNNING;
      break;
    case rtx::JobState::kCompleted:
      target.state = RTX_JOB_COMPLETED;
      break;
    case rtx::JobState::kFailed:
      target.state = RTX_JOB_FAILED;
      break;
    case rtx::JobState::kCancelled:
      target.state = RTX_JOB_CANCELLED;
      break;
  }
  target.total_progress = source.total_progress;
  target.phase_progress = source.phase_progress;
  target.processed_frames = source.processed_frames;
  target.total_frames = source.total_frames;
  target.fps = source.fps;
  target.elapsed_seconds = source.elapsed_seconds;
  target.eta_seconds = source.eta_seconds;
  CopyUtf8(source.current_phase, target.current_phase);
  CopyUtf8(source.status_message, target.status_message);
}

void FillEvent(const rtx::LogEvent& source, RtxEventV1& target) {
  switch (source.severity) {
    case rtx::EventSeverity::kError:
      target.severity = RTX_EVENT_ERROR;
      break;
    case rtx::EventSeverity::kWarn:
      target.severity = RTX_EVENT_WARN;
      break;
    case rtx::EventSeverity::kInfo:
      target.severity = RTX_EVENT_INFO;
      break;
    case rtx::EventSeverity::kDebug:
      target.severity = RTX_EVENT_DEBUG;
      break;
  }
  target.timestamp_ms = source.timestamp_ms;
  CopyUtf8(source.phase, target.phase);
  CopyUtf8(source.message, target.message);
}

}  // namespace

RTX_API std::uint32_t rtx_init_library() {
  auto& state = GetState();
  std::scoped_lock lock(state.mutex);

  if (state.initialized && state.job_manager) {
    return RTX_STATUS_OK;
  }

  state.runtime_paths = rtx::ResolveRuntimePaths();
  state.job_manager = std::make_unique<rtx::JobManager>();
  state.initialized = true;
  state.last_error.clear();

  rtx::Logger logger(rtx::LogLevel::kError);
  auto status = rtx::ProbeCurrentSystem(logger, state.system_probe);
  if (!status.ok()) {
    state.last_error = status.message();
    return ToApiStatus(status);
  }

  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_shutdown_library() {
  auto& state = GetState();
  std::scoped_lock lock(state.mutex);
  state.job_manager.reset();
  state.system_probe = rtx::SystemProbe{};
  state.runtime_paths = rtx::RuntimePaths{};
  state.initialized = false;
  state.last_error.clear();
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_probe_system(RtxSystemProbeV1* probe) {
  if (probe == nullptr) {
    SetLastError("probe buffer is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  auto& state = GetState();
  std::scoped_lock lock(state.mutex);
  if (!state.initialized) {
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }

  FillSystemProbe(state.system_probe, *probe);
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_list_supported_codecs(RtxCodecSupportV1* codecs,
                                                std::uint32_t capacity,
                                                std::uint32_t* count) {
  if (count == nullptr) {
    SetLastError("count pointer is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  rtx::Logger logger(rtx::LogLevel::kError);
  rtx::FfmpegProbe probe(logger);
  struct CodecInfo {
    const char* name;
    bool decode;
    bool encode;
  };
  const CodecInfo infos[] = {
      {"h264", probe.CanDecodeWithHardware("h264"), probe.CanEncodeWithHardware("h264")},
      {"hevc", probe.CanDecodeWithHardware("hevc"), probe.CanEncodeWithHardware("hevc")},
      {"av1", probe.CanDecodeWithHardware("av1"), probe.CanEncodeWithHardware("av1")},
  };

  *count = static_cast<std::uint32_t>(std::size(infos));
  if (codecs == nullptr || capacity == 0) {
    return RTX_STATUS_OK;
  }

  const auto limit = std::min<std::uint32_t>(capacity, *count);
  for (std::uint32_t index = 0; index < limit; ++index) {
    std::memset(&codecs[index], 0, sizeof(RtxCodecSupportV1));
    CopyUtf8(std::string(infos[index].name), codecs[index].codec_name);
    codecs[index].hardware_decode = infos[index].decode ? 1 : 0;
    codecs[index].hardware_encode = infos[index].encode ? 1 : 0;
  }
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_analyze_input(const char* input_path, RtxMediaInfoV1* media_info) {
  if (input_path == nullptr || media_info == nullptr) {
    SetLastError("input_path or media_info is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  rtx::Logger logger(rtx::LogLevel::kError);
  rtx::FfmpegProbe probe(logger);
  rtx::MediaInfo info;
  const auto status = probe.Probe(input_path, info);
  if (!status.ok()) {
    SetLastError(status.message());
    return ToApiStatus(status);
  }

  FillMediaInfo(info, *media_info);
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_create_job(const RtxJobConfigV1* config, std::uint64_t* job_id) {
  if (config == nullptr || job_id == nullptr) {
    SetLastError("config or job_id is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }

  auto job_config = ConvertConfig(*config);
  auto* job_manager = state.job_manager.get();
  lock.unlock();

  const auto status = job_manager->CreateJob(job_config, *job_id);
  if (!status.ok()) {
    SetLastError(status.message());
  }
  return ToApiStatus(status);
}

RTX_API std::uint32_t rtx_start_job(std::uint64_t job_id) {
  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }

  auto* job_manager = state.job_manager.get();
  const auto runtime_paths = state.runtime_paths;
  const auto system_probe = state.system_probe;
  lock.unlock();

  const auto status = job_manager->StartJob(job_id, runtime_paths, system_probe);
  if (!status.ok()) {
    SetLastError(status.message());
  }
  return ToApiStatus(status);
}

RTX_API std::uint32_t rtx_query_job_progress(std::uint64_t job_id, RtxJobProgressV1* progress) {
  if (progress == nullptr) {
    SetLastError("progress is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }
  auto* job_manager = state.job_manager.get();
  lock.unlock();

  rtx::JobProgress native_progress;
  const auto status = job_manager->QueryJobProgress(job_id, native_progress);
  if (!status.ok()) {
    SetLastError(status.message());
    return ToApiStatus(status);
  }

  FillProgress(native_progress, *progress);
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_cancel_job(std::uint64_t job_id) {
  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }
  auto* job_manager = state.job_manager.get();
  lock.unlock();

  const auto status = job_manager->CancelJob(job_id);
  if (!status.ok()) {
    SetLastError(status.message());
  }
  return ToApiStatus(status);
}

RTX_API std::uint32_t rtx_destroy_job(std::uint64_t job_id) {
  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }
  auto* job_manager = state.job_manager.get();
  lock.unlock();

  const auto status = job_manager->DestroyJob(job_id);
  if (!status.ok()) {
    SetLastError(status.message());
  }
  return ToApiStatus(status);
}

RTX_API std::uint32_t rtx_drain_job_events(std::uint64_t job_id,
                                           RtxEventV1* events,
                                           std::uint32_t capacity,
                                           std::uint32_t* count) {
  if (count == nullptr) {
    SetLastError("count is null");
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  auto& state = GetState();
  std::unique_lock lock(state.mutex);
  if (!state.initialized || !state.job_manager) {
    lock.unlock();
    SetLastError("library not initialized");
    return RTX_STATUS_UNAVAILABLE;
  }
  auto* job_manager = state.job_manager.get();
  lock.unlock();

  std::vector<rtx::LogEvent> native_events;
  const auto status = job_manager->DrainEvents(job_id, capacity, native_events);
  if (!status.ok()) {
    SetLastError(status.message());
    return ToApiStatus(status);
  }

  *count = static_cast<std::uint32_t>(native_events.size());
  if (events != nullptr) {
    for (std::size_t index = 0; index < native_events.size(); ++index) {
      std::memset(&events[index], 0, sizeof(RtxEventV1));
      FillEvent(native_events[index], events[index]);
    }
  }
  return RTX_STATUS_OK;
}

RTX_API std::uint32_t rtx_get_last_error(char* buffer, std::uint32_t buffer_size) {
  if (buffer == nullptr || buffer_size == 0) {
    return RTX_STATUS_INVALID_ARGUMENT;
  }

  auto& state = GetState();
  std::scoped_lock lock(state.mutex);
  std::memset(buffer, 0, buffer_size);
  const auto copy_count = std::min<std::size_t>(buffer_size - 1, state.last_error.size());
  std::memcpy(buffer, state.last_error.data(), copy_count);
  return RTX_STATUS_OK;
}
