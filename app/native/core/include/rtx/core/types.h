#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "rtx/core/logger.h"

namespace rtx {

enum class OutputContainer {
  kMp4 = 0,
  kMkv = 1,
};

enum class VideoCodec {
  kHevc = 0,
  kAv1 = 1,
  kH264 = 2,
};

enum class AudioMode {
  kCopyPreferred = 0,
  kAac = 1,
  kDisabled = 2,
};

enum class QualityPreset {
  kFast = 0,
  kBalanced = 1,
  kHighQuality = 2,
};

enum class VideoRateControlMode {
  kAuto = 0,
  kTargetBitrate = 1,
  kConstantQuality = 2,
};

enum class JobState {
  kCreated = 0,
  kQueued = 1,
  kAnalyzing = 2,
  kPreparing = 3,
  kRunning = 4,
  kCompleted = 5,
  kFailed = 6,
  kCancelled = 7,
};

enum class EventSeverity {
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
};

struct RuntimePaths {
  std::filesystem::path ffmpeg_root;
  std::filesystem::path ffmpeg_bin_dir;
  std::filesystem::path ffmpeg_exe;
  std::filesystem::path ffprobe_exe;
  std::filesystem::path flutter_root;
  std::filesystem::path flutter_exe;
  std::filesystem::path cmake_exe;
  std::filesystem::path nvidia_sdk_root;
  std::filesystem::path nvidia_sdk_runtime_dir;
  std::filesystem::path video_codec_interface_root;
};

struct ColorInfo {
  std::string primaries = "unknown";
  std::string transfer = "unknown";
  std::string matrix = "unknown";
  std::string range = "unknown";
  std::string pixel_format = "unknown";
  int bit_depth = 8;
  bool hdr_signaled = false;
};

struct VideoStreamInfo {
  int index = -1;
  std::string codec_name;
  int width = 0;
  int height = 0;
  double frame_rate = 0.0;
  int frame_rate_num = 0;
  int frame_rate_den = 0;
  std::int64_t frame_count = 0;
  ColorInfo color;
};

struct AudioStreamInfo {
  int index = -1;
  std::string codec_name;
  int channels = 0;
  int sample_rate = 0;
};

struct MediaInfo {
  std::string path;
  std::string container_name;
  double duration_seconds = 0.0;
  bool has_video = false;
  bool has_audio = false;
  VideoStreamInfo primary_video;
  AudioStreamInfo primary_audio;
};

struct JobConfig {
  std::string input_path;
  std::string output_path;
  OutputContainer output_container = OutputContainer::kMkv;
  VideoCodec output_video_codec = VideoCodec::kHevc;
  AudioMode output_audio_mode = AudioMode::kCopyPreferred;
  bool upscale_enabled = false;
  std::uint32_t upscale_factor = 2;
  int target_width = 0;
  int target_height = 0;
  bool lock_aspect_ratio = true;
  bool hdr_enabled = false;
  std::uint32_t truehdr_contrast = 100;
  std::uint32_t truehdr_saturation = 100;
  std::uint32_t truehdr_middle_gray = 50;
  std::uint32_t truehdr_max_luminance = 1000;
  QualityPreset quality_preset = QualityPreset::kBalanced;
  VideoRateControlMode video_rate_control_mode = VideoRateControlMode::kAuto;
  std::uint32_t target_video_bitrate_kbps = 12000;
  std::uint32_t constant_quality = 18;
  int gpu_index = 0;
  bool keep_audio = true;
  bool overwrite_output = false;
  LogLevel logging_level = LogLevel::kInfo;
};

struct SystemProbe {
  std::string os_version;
  std::string gpu_name;
  std::string driver_version;
  std::string cuda_version;
  bool has_nvidia_gpu = false;
  bool ngx_vsr_available = false;
  bool ngx_truehdr_available = false;
  bool hdr_export_available = false;
  bool h264_hw_decode = false;
  bool hevc_hw_decode = false;
  bool av1_hw_decode = false;
  bool h264_hw_encode = false;
  bool hevc_hw_encode = false;
  bool av1_hw_encode = false;
  std::string supported_pixel_formats;
  std::string notes;
  RuntimePaths runtime_paths;
};

struct JobProgress {
  JobState state = JobState::kCreated;
  double total_progress = 0.0;
  double phase_progress = 0.0;
  std::string current_phase = "空闲";
  std::string status_message = "就绪";
  std::uint64_t processed_frames = 0;
  std::uint64_t total_frames = 0;
  double fps = 0.0;
  double elapsed_seconds = 0.0;
  double eta_seconds = -1.0;
};

struct LogEvent {
  EventSeverity severity = EventSeverity::kInfo;
  std::uint64_t timestamp_ms = 0;
  std::string phase;
  std::string message;
};

struct FramePacket {
  void* data = nullptr;
  std::size_t size_bytes = 0;
  int width = 0;
  int height = 0;
  int pitch = 0;
  std::string pixel_format;
};

const char* ToString(OutputContainer value);
const char* ToString(VideoCodec value);
const char* ToString(AudioMode value);
const char* ToString(QualityPreset value);
const char* ToString(VideoRateControlMode value);
const char* ToString(JobState value);
const char* ToString(LogLevel value);
EventSeverity ToEventSeverity(LogLevel value);

}  // namespace rtx
