#pragma once

#include <cstdint>

#if defined(_WIN32)
#define RTX_API extern "C" __declspec(dllexport)
#else
#define RTX_API extern "C"
#endif

enum RtxStatusCode : std::uint32_t {
  RTX_STATUS_OK = 0,
  RTX_STATUS_INVALID_ARGUMENT = 1,
  RTX_STATUS_NOT_FOUND = 2,
  RTX_STATUS_UNAVAILABLE = 3,
  RTX_STATUS_DEPENDENCY_MISSING = 4,
  RTX_STATUS_UNSUPPORTED = 5,
  RTX_STATUS_IO_ERROR = 6,
  RTX_STATUS_INTERNAL = 7,
  RTX_STATUS_CANCELLED = 8,
};

enum RtxJobState : std::uint32_t {
  RTX_JOB_CREATED = 0,
  RTX_JOB_QUEUED = 1,
  RTX_JOB_ANALYZING = 2,
  RTX_JOB_PREPARING = 3,
  RTX_JOB_RUNNING = 4,
  RTX_JOB_COMPLETED = 5,
  RTX_JOB_FAILED = 6,
  RTX_JOB_CANCELLED = 7,
};

enum RtxOutputContainer : std::uint32_t {
  RTX_CONTAINER_MP4 = 0,
  RTX_CONTAINER_MKV = 1,
};

enum RtxVideoCodec : std::uint32_t {
  RTX_CODEC_HEVC = 0,
  RTX_CODEC_AV1 = 1,
  RTX_CODEC_H264 = 2,
};

enum RtxAudioMode : std::uint32_t {
  RTX_AUDIO_COPY_PREFERRED = 0,
  RTX_AUDIO_AAC = 1,
  RTX_AUDIO_DISABLED = 2,
};

enum RtxQualityPreset : std::uint32_t {
  RTX_QUALITY_FAST = 0,
  RTX_QUALITY_BALANCED = 1,
  RTX_QUALITY_HIGH = 2,
};

enum RtxVideoRateControlMode : std::uint32_t {
  RTX_RATE_CONTROL_AUTO = 0,
  RTX_RATE_CONTROL_TARGET_BITRATE = 1,
  RTX_RATE_CONTROL_CONSTANT_QUALITY = 2,
};

enum RtxLogLevel : std::uint32_t {
  RTX_LOG_ERROR = 0,
  RTX_LOG_WARN = 1,
  RTX_LOG_INFO = 2,
  RTX_LOG_DEBUG = 3,
};

enum RtxEventSeverity : std::uint32_t {
  RTX_EVENT_ERROR = 0,
  RTX_EVENT_WARN = 1,
  RTX_EVENT_INFO = 2,
  RTX_EVENT_DEBUG = 3,
};

struct RtxJobConfigV1 {
  std::uint32_t struct_size;
  const char* input_path;
  const char* output_path;
  std::uint32_t output_container;
  std::uint32_t output_video_codec;
  std::uint32_t output_audio_mode;
  std::uint32_t quality_preset;
  std::int32_t target_width;
  std::int32_t target_height;
  std::uint32_t upscale_enabled;
  std::uint32_t upscale_factor;
  std::uint32_t hdr_enabled;
  std::uint32_t truehdr_contrast;
  std::uint32_t truehdr_saturation;
  std::uint32_t truehdr_middle_gray;
  std::uint32_t truehdr_max_luminance;
  std::uint32_t video_rate_control_mode;
  std::uint32_t target_video_bitrate_kbps;
  std::uint32_t constant_quality;
  std::int32_t gpu_index;
  std::uint32_t keep_audio;
  std::uint32_t overwrite_output;
  std::uint32_t logging_level;
  std::uint32_t lock_aspect_ratio;
};

struct RtxSystemProbeV1 {
  std::uint32_t struct_size;
  char os_version[128];
  char gpu_name[128];
  char driver_version[64];
  char cuda_version[64];
  std::uint32_t has_nvidia_gpu;
  std::uint32_t ngx_vsr_available;
  std::uint32_t ngx_truehdr_available;
  std::uint32_t hdr_export_available;
  std::uint32_t h264_hw_decode;
  std::uint32_t hevc_hw_decode;
  std::uint32_t av1_hw_decode;
  std::uint32_t h264_hw_encode;
  std::uint32_t hevc_hw_encode;
  std::uint32_t av1_hw_encode;
  char supported_pixel_formats[128];
  char notes[256];
};

struct RtxMediaInfoV1 {
  std::uint32_t struct_size;
  char container_name[64];
  char video_codec[32];
  char audio_codec[32];
  char pixel_format[32];
  char color_primaries[32];
  char transfer[32];
  char matrix[32];
  char color_range[16];
  std::int32_t width;
  std::int32_t height;
  double frame_rate;
  double duration_seconds;
  std::uint64_t frame_count;
  std::int32_t bit_depth;
  std::int32_t audio_channels;
  std::int32_t audio_sample_rate;
  std::uint32_t has_audio;
  std::uint32_t hdr_signaled;
};

struct RtxJobProgressV1 {
  std::uint32_t struct_size;
  std::uint32_t state;
  double total_progress;
  double phase_progress;
  std::uint64_t processed_frames;
  std::uint64_t total_frames;
  double fps;
  double elapsed_seconds;
  double eta_seconds;
  char current_phase[64];
  char status_message[256];
};

struct RtxEventV1 {
  std::uint32_t severity;
  std::uint64_t timestamp_ms;
  char phase[64];
  char message[256];
};

struct RtxCodecSupportV1 {
  char codec_name[32];
  std::uint32_t hardware_decode;
  std::uint32_t hardware_encode;
};

RTX_API std::uint32_t rtx_init_library();
RTX_API std::uint32_t rtx_shutdown_library();
RTX_API std::uint32_t rtx_probe_system(RtxSystemProbeV1* probe);
RTX_API std::uint32_t rtx_list_supported_codecs(RtxCodecSupportV1* codecs,
                                                std::uint32_t capacity,
                                                std::uint32_t* count);
RTX_API std::uint32_t rtx_analyze_input(const char* input_path, RtxMediaInfoV1* media_info);
RTX_API std::uint32_t rtx_create_job(const RtxJobConfigV1* config, std::uint64_t* job_id);
RTX_API std::uint32_t rtx_start_job(std::uint64_t job_id);
RTX_API std::uint32_t rtx_query_job_progress(std::uint64_t job_id, RtxJobProgressV1* progress);
RTX_API std::uint32_t rtx_cancel_job(std::uint64_t job_id);
RTX_API std::uint32_t rtx_destroy_job(std::uint64_t job_id);
RTX_API std::uint32_t rtx_drain_job_events(std::uint64_t job_id,
                                           RtxEventV1* events,
                                           std::uint32_t capacity,
                                           std::uint32_t* count);
RTX_API std::uint32_t rtx_get_last_error(char* buffer, std::uint32_t buffer_size);
