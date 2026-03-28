#include "rtx/pipeline/processing_pipeline.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "rtx/core/validation.h"
#include "rtx/enhance/enhancement_backend.h"
#include "rtx/video_io/ffmpeg_runtime.h"

namespace rtx {

namespace {

struct PipeProcess {
  HANDLE stream = INVALID_HANDLE_VALUE;
  PROCESS_INFORMATION process{};
  std::filesystem::path log_path;

  ~PipeProcess() {
    if (stream != INVALID_HANDLE_VALUE) {
      CloseHandle(stream);
    }
    if (process.hProcess != nullptr) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 1000);
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
    }
  }
};

std::wstring Quote(const std::filesystem::path& path) { return L"\"" + path.native() + L"\""; }

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  std::ostringstream output;
  output << file.rdbuf();
  return output.str();
}

std::filesystem::path MakeTemporaryOutputPath(const std::filesystem::path& output_path) {
  return output_path.parent_path() /
         (output_path.stem().string() + ".working" + output_path.extension().string());
}

std::string FrameRateString(const VideoStreamInfo& video) {
  if (video.frame_rate_num > 0 && video.frame_rate_den > 0) {
    return std::to_string(video.frame_rate_num) + "/" + std::to_string(video.frame_rate_den);
  }

  std::ostringstream stream;
  stream.setf(std::ios::fixed);
  stream.precision(6);
  stream << (video.frame_rate > 0.0 ? video.frame_rate : 30.0);
  return stream.str();
}

std::uint64_t EstimateFrameCount(const MediaInfo& input) {
  if (input.primary_video.frame_count > 0) {
    return static_cast<std::uint64_t>(input.primary_video.frame_count);
  }
  if (input.primary_video.frame_rate > 0.0 && input.duration_seconds > 0.0) {
    return static_cast<std::uint64_t>(
        std::llround(input.primary_video.frame_rate * input.duration_seconds));
  }
  return 0;
}

std::string NvencPreset(QualityPreset preset) {
  switch (preset) {
    case QualityPreset::kFast:
      return "p3";
    case QualityPreset::kBalanced:
      return "p5";
    case QualityPreset::kHighQuality:
      return "p7";
  }
  return "p5";
}

std::wstring BuildNvencRateControlArgs(const JobConfig& config) {
  std::wostringstream args;
  switch (config.video_rate_control_mode) {
    case VideoRateControlMode::kAuto:
      break;
    case VideoRateControlMode::kTargetBitrate: {
      const auto bitrate = std::to_wstring(config.target_video_bitrate_kbps) + L"k";
      const auto maxrate = std::to_wstring(
                               static_cast<std::uint32_t>(config.target_video_bitrate_kbps * 3 / 2)) +
                           L"k";
      const auto bufsize =
          std::to_wstring(static_cast<std::uint32_t>(config.target_video_bitrate_kbps * 2)) + L"k";
      args << L" -rc vbr_hq -b:v " << bitrate << L" -maxrate " << maxrate << L" -bufsize " << bufsize;
      break;
    }
    case VideoRateControlMode::kConstantQuality:
      args << L" -rc vbr_hq -cq " << config.constant_quality << L" -b:v 0";
      break;
  }
  return args.str();
}

std::wstring BuildDecoderCommand(const RuntimePaths& paths,
                                 const JobConfig& config,
                                 const MediaInfo& input,
                                 int decode_width,
                                 int decode_height,
                                 bool decoder_scales_frame,
                                 const SystemProbe& system_probe) {
  std::wostringstream command;
  command << Quote(paths.ffmpeg_exe) << L" -hide_banner -loglevel error -nostdin "
          << L" -i " << Quote(std::filesystem::path(config.input_path))
          << L" -map 0:v:0 -an -sn -dn -vsync 0 ";

  if (decoder_scales_frame) {
    command << L" -vf \"scale=" << decode_width << L":" << decode_height
            << L":flags=lanczos,format=rgba\"";
  } else {
    command << L" -pix_fmt rgba";
  }

  command << L" -f rawvideo -";
  return command.str();
}

std::string ChooseEncoderName(const JobConfig& config) {
  if (config.output_video_codec == VideoCodec::kAv1) {
    return "av1_nvenc";
  }
  if (config.output_video_codec == VideoCodec::kH264) {
    return "h264_nvenc";
  }
  return "hevc_nvenc";
}

bool IsMp4SafeAudioCopy(const AudioStreamInfo& audio) {
  return audio.codec_name == "aac" || audio.codec_name == "ac3" || audio.codec_name == "eac3" ||
         audio.codec_name == "alac" || audio.codec_name == "mp3";
}

std::wstring BuildEncoderCommand(const RuntimePaths& paths,
                                 const JobConfig& config,
                                 const MediaInfo& input,
                                 const std::filesystem::path& output_path,
                                 int output_width,
                                 int output_height,
                                 bool hdr_output) {
  const std::string encoder_name = ChooseEncoderName(config);
  const std::string preset = NvencPreset(config.quality_preset);
  const std::string frame_rate = FrameRateString(input.primary_video);
  const bool copy_audio =
      config.keep_audio && input.has_audio && config.output_audio_mode == AudioMode::kCopyPreferred &&
      (config.output_container == OutputContainer::kMkv || IsMp4SafeAudioCopy(input.primary_audio));
  const bool transcode_audio =
      config.keep_audio && input.has_audio && (config.output_audio_mode == AudioMode::kAac ||
                                               (config.output_audio_mode == AudioMode::kCopyPreferred && !copy_audio));

  std::wostringstream command;
  command << Quote(paths.ffmpeg_exe) << L" -hide_banner -loglevel error -nostdin -y"
          << L" -f rawvideo -pix_fmt " << (hdr_output ? L"x2bgr10le" : L"rgba") << L" -video_size "
          << output_width << L"x" << output_height << L" -framerate "
          << std::wstring(frame_rate.begin(), frame_rate.end()) << L" -i - -i "
          << Quote(std::filesystem::path(config.input_path)) << L" -map 0:v:0";

  if (config.keep_audio && input.has_audio) {
    command << L" -map 1:a?";
  }
  command << L" -map_metadata 1 -map_chapters 1";

  if (hdr_output) {
    command << L" -vf \"setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc:range=tv,format=p010le\"";
    command << L" -c:v " << std::wstring(encoder_name.begin(), encoder_name.end())
            << L" -preset " << std::wstring(preset.begin(), preset.end()) << L" -tune hq"
            << L" -rc-lookahead 20 -spatial_aq 1 -aq-strength 10"
            << BuildNvencRateControlArgs(config) << L" -pix_fmt p010le";
    if (encoder_name == "hevc_nvenc") {
      command << L" -profile:v main10";
    }
    command << L" -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc";
  } else {
    command << L" -vf \"format=yuv420p\""
            << L" -c:v " << std::wstring(encoder_name.begin(), encoder_name.end())
            << L" -preset " << std::wstring(preset.begin(), preset.end()) << L" -tune hq"
            << L" -rc-lookahead 20 -spatial_aq 1 -aq-strength 10"
            << BuildNvencRateControlArgs(config) << L" -pix_fmt yuv420p";
  }

  if (copy_audio) {
    command << L" -c:a copy";
  } else if (transcode_audio) {
    command << L" -c:a aac -b:a 192k";
  } else {
    command << L" -an";
  }

  if (config.output_container == OutputContainer::kMp4) {
    if (encoder_name == "hevc_nvenc") {
      command << L" -tag:v hvc1";
    }
    command << L" -movflags +faststart";
  }

  command << L" " << Quote(output_path);
  return command.str();
}

Status OpenPipeProcess(const std::wstring& command,
                       bool read_from_child,
                       const std::filesystem::path& log_path,
                       PipeProcess& pipe_process) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;

  HANDLE read_pipe = INVALID_HANDLE_VALUE;
  HANDLE write_pipe = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) {
    return Status::Unavailable("创建 FFmpeg 管道失败。");
  }

  HANDLE parent_stream = read_from_child ? read_pipe : write_pipe;
  HANDLE child_stream = read_from_child ? write_pipe : read_pipe;
  if (!SetHandleInformation(parent_stream, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(read_pipe);
    CloseHandle(write_pipe);
    return Status::Unavailable("设置 FFmpeg 管道属性失败。");
  }

  HANDLE log_handle = CreateFileW(log_path.native().c_str(), GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (log_handle == INVALID_HANDLE_VALUE) {
    CloseHandle(read_pipe);
    CloseHandle(write_pipe);
    return Status::Unavailable("创建 FFmpeg 日志文件失败。");
  }

  HANDLE null_handle = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdError = log_handle;
  if (read_from_child) {
    startup.hStdInput = null_handle;
    startup.hStdOutput = child_stream;
  } else {
    startup.hStdInput = child_stream;
    startup.hStdOutput = null_handle;
  }

  PROCESS_INFORMATION process{};
  std::wstring mutable_command = command;
  const BOOL created = CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);

  CloseHandle(log_handle);
  CloseHandle(null_handle);
  CloseHandle(child_stream);

  if (!created) {
    CloseHandle(parent_stream);
    return Status::Unavailable("启动 FFmpeg 处理进程失败。");
  }

  pipe_process.stream = parent_stream;
  pipe_process.process = process;
  pipe_process.log_path = log_path;
  return Status::Ok();
}

Status ReadExact(HANDLE stream, std::vector<unsigned char>& buffer, bool& eof) {
  eof = false;
  std::size_t offset = 0;
  while (offset < buffer.size()) {
    DWORD bytes_read = 0;
    const BOOL ok = ReadFile(stream, buffer.data() + offset,
                             static_cast<DWORD>(buffer.size() - offset), &bytes_read, nullptr);
    if (!ok) {
      if (GetLastError() == ERROR_BROKEN_PIPE && offset == 0) {
        eof = true;
        return Status::Ok();
      }
      return Status::IoError("读取解码后帧数据失败。");
    }
    if (bytes_read == 0) {
      eof = (offset == 0);
      if (offset == 0) {
        return Status::Ok();
      }
      return Status::IoError("FFmpeg 输出了不完整的帧数据。");
    }
    offset += bytes_read;
  }
  return Status::Ok();
}

Status WriteExact(HANDLE stream, const std::vector<unsigned char>& buffer) {
  std::size_t offset = 0;
  while (offset < buffer.size()) {
    DWORD bytes_written = 0;
    const BOOL ok = WriteFile(stream, buffer.data() + offset,
                              static_cast<DWORD>(buffer.size() - offset), &bytes_written, nullptr);
    if (!ok || bytes_written == 0) {
      return Status::IoError("向编码器写入帧数据失败。");
    }
    offset += bytes_written;
  }
  return Status::Ok();
}

Status WaitProcess(PipeProcess& pipe_process, const std::string& stage) {
  if (pipe_process.stream != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_process.stream);
    pipe_process.stream = INVALID_HANDLE_VALUE;
  }

  WaitForSingleObject(pipe_process.process.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(pipe_process.process.hProcess, &exit_code);
  CloseHandle(pipe_process.process.hThread);
  CloseHandle(pipe_process.process.hProcess);
  pipe_process.process = {};

  if (exit_code == 0) {
    return Status::Ok();
  }

  std::string message = stage + " 阶段的 FFmpeg 进程失败，退出码: " + std::to_string(exit_code);
  const std::string log_text = ReadTextFile(pipe_process.log_path);
  if (!log_text.empty()) {
    message += "。详情: " + log_text;
  }
  return Status::Unavailable(message);
}

}  // namespace

ProcessingPipeline::ProcessingPipeline(const RuntimePaths& runtime_paths, Logger& logger)
    : runtime_paths_(runtime_paths), logger_(logger) {}

Status ProcessingPipeline::Run(const JobConfig& config,
                               const SystemProbe& system_probe,
                               const MediaInfo& input,
                               const ProcessingCallbacks& callbacks) {
  auto emit_progress = [&](JobState state,
                           const std::string& phase,
                           double total_progress,
                           double phase_progress,
                           std::uint64_t processed_frames,
                           std::uint64_t total_frames,
                           double fps,
                           double elapsed_seconds,
                           double eta_seconds,
                           const std::string& message) {
    if (callbacks.on_progress) {
      callbacks.on_progress(state, phase, total_progress, phase_progress, processed_frames,
                            total_frames, fps, elapsed_seconds, eta_seconds, message);
    }
  };

  emit_progress(JobState::kPreparing, "配置校验", 0.02, 0.25, 0, 0, 0.0, 0.0, -1.0,
                "正在校验任务配置");

  auto status = ValidateHdrRequest(config, input);
  if (!status.ok()) {
    return status;
  }

  int output_width = 0;
  int output_height = 0;
  bool decoder_scales_frame = false;
  status = ResolveOutputResolution(config, input, output_width, output_height, decoder_scales_frame);
  if (!status.ok()) {
    return status;
  }

  status = EnsureProcessRuntimePath(runtime_paths_);
  if (!status.ok()) {
    return status;
  }

  emit_progress(JobState::kPreparing, "增强后端", 0.08, 0.5, 0, 0, 0.0, 0.0, -1.0,
                "正在创建增强后端");

  std::unique_ptr<IEnhancementBackend> backend =
      (config.hdr_enabled || config.upscale_enabled) ? CreateNvidiaCudaNgxBackend()
                                                     : CreateNullEnhancementBackend();
  std::unique_ptr<IEnhancementSession> session;
  status = backend->CreateSession(config, input, output_width, output_height, logger_, session);
  if (!status.ok()) {
    return status;
  }

  const bool hdr_output = config.hdr_enabled;
  const std::filesystem::path output_path(config.output_path);
  const std::filesystem::path temporary_output = MakeTemporaryOutputPath(output_path);
  std::error_code remove_error;
  if (std::filesystem::exists(temporary_output)) {
    std::filesystem::remove(temporary_output, remove_error);
  }

  const auto decoder_command =
      BuildDecoderCommand(runtime_paths_, config, input,
                          decoder_scales_frame ? output_width : input.primary_video.width,
                          decoder_scales_frame ? output_height : input.primary_video.height,
                          decoder_scales_frame, system_probe);
  const auto encoder_command =
      BuildEncoderCommand(runtime_paths_, config, input, temporary_output, output_width, output_height,
                          hdr_output);

  const auto scratch_dir =
      std::filesystem::temp_directory_path() / ("rtxhdr_rtxvsr_" + std::to_string(GetCurrentProcessId()));
  std::filesystem::create_directories(scratch_dir);

  PipeProcess decoder_process;
  status = OpenPipeProcess(decoder_command, true, scratch_dir / "decoder.log", decoder_process);
  if (!status.ok()) {
    return status;
  }

  PipeProcess encoder_process;
  status = OpenPipeProcess(encoder_command, false, scratch_dir / "encoder.log", encoder_process);
  if (!status.ok()) {
    return status;
  }

  const int input_width = decoder_scales_frame ? output_width : input.primary_video.width;
  const int input_height = decoder_scales_frame ? output_height : input.primary_video.height;

  const std::size_t input_bytes =
      static_cast<std::size_t>(input_width) * static_cast<std::size_t>(input_height) * 4;
  const std::size_t output_bytes =
      static_cast<std::size_t>(output_width) * static_cast<std::size_t>(output_height) * 4;

  std::vector<unsigned char> input_frame(input_bytes);
  std::vector<unsigned char> output_frame(output_bytes);

  FramePacket input_packet{input_frame.data(), input_frame.size(), input_width, input_height,
                           input_width * 4, "rgba"};
  FramePacket output_packet{output_frame.data(), output_frame.size(), output_width, output_height,
                            output_width * 4, hdr_output ? "x2bgr10le" : "rgba"};

  const auto total_frames = EstimateFrameCount(input);
  const auto start_time = std::chrono::steady_clock::now();

  emit_progress(JobState::kRunning, "帧处理", 0.10, 0.0, 0, total_frames, 0.0, 0.0, -1.0,
                "正在处理视频帧");

  std::uint64_t processed_frames = 0;
  while (true) {
    if (callbacks.is_cancelled && callbacks.is_cancelled()) {
      return Status::Cancelled("任务已取消。");
    }

    bool eof = false;
    status = ReadExact(decoder_process.stream, input_frame, eof);
    if (!status.ok()) {
      return status;
    }
    if (eof) {
      break;
    }

    status = session->ProcessFrame(input_packet, output_packet);
    if (!status.ok()) {
      return status;
    }

    status = WriteExact(encoder_process.stream, output_frame);
    if (!status.ok()) {
      return status;
    }

    ++processed_frames;
    if (processed_frames == 1 || processed_frames % 15 == 0) {
      const auto now = std::chrono::steady_clock::now();
      const double elapsed_seconds =
          std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time).count();
      const double fps =
          elapsed_seconds > 0.0 ? static_cast<double>(processed_frames) / elapsed_seconds : 0.0;
      double eta_seconds = -1.0;
      double phase_progress = 0.0;
      if (total_frames > 0) {
        phase_progress = std::min(1.0, static_cast<double>(processed_frames) /
                                           static_cast<double>(total_frames));
        if (fps > 0.0 && processed_frames < total_frames) {
          eta_seconds = static_cast<double>(total_frames - processed_frames) / fps;
        }
      }

      emit_progress(JobState::kRunning, "帧处理", 0.10 + phase_progress * 0.82, phase_progress,
                    processed_frames, total_frames, fps, elapsed_seconds, eta_seconds,
                    "正在持续处理视频帧");
    }
  }

  status = WaitProcess(decoder_process, "解码");
  if (!status.ok()) {
    return status;
  }

  status = WaitProcess(encoder_process, "编码");
  if (!status.ok()) {
    return status;
  }

  emit_progress(JobState::kRunning, "输出校验", 0.95, 0.5, processed_frames, total_frames, 0.0, 0.0,
                -1.0, "正在校验输出文件");

  if (!std::filesystem::exists(temporary_output) || std::filesystem::file_size(temporary_output) == 0) {
    return Status::IoError("导出失败，输出文件不存在或大小为 0。");
  }

  if (std::filesystem::exists(output_path) && config.overwrite_output) {
    std::filesystem::remove(output_path, remove_error);
  }
  std::filesystem::rename(temporary_output, output_path, remove_error);
  if (remove_error) {
    return Status::IoError("输出文件重命名失败: " + remove_error.message());
  }

  emit_progress(JobState::kCompleted, "已完成", 1.0, 1.0, processed_frames, total_frames, 0.0, 0.0,
                0.0, "导出已成功完成");
  return Status::Ok();
}

}  // namespace rtx
