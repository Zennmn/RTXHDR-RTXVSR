#include "rtx/video_io/ffmpeg_runtime.h"

#include <windows.h>

#include <cstdlib>
#include <string>

namespace rtx {

namespace {

std::filesystem::path GetExecutableDirectory() {
  wchar_t buffer[MAX_PATH] = {};
  const auto length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path FindOnPath(const wchar_t* executable) {
  wchar_t resolved[MAX_PATH] = {};
  if (SearchPathW(nullptr, executable, nullptr, MAX_PATH, resolved, nullptr) > 0) {
    return std::filesystem::path(resolved);
  }
  return {};
}

std::filesystem::path ExpandEnvPath(const wchar_t* name) {
  wchar_t* value = nullptr;
  std::size_t length = 0;
  if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr) {
    return {};
  }
  std::filesystem::path result(value);
  free(value);
  return result;
}

void PrependPathIfMissing(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }

  wchar_t* value = nullptr;
  std::size_t length = 0;
  std::wstring current;
  if (_wdupenv_s(&value, &length, L"PATH") == 0 && value != nullptr) {
    current = value;
    free(value);
  }

  const std::wstring wanted = path.native();
  if (current.find(wanted) != std::wstring::npos) {
    return;
  }

  const std::wstring updated = wanted + L";" + current;
  _wputenv_s(L"PATH", updated.c_str());
}

}  // namespace

RuntimePaths ResolveRuntimePaths() {
  RuntimePaths paths;
  const auto executable_dir = GetExecutableDirectory();

  const auto ffmpeg_root_env = ExpandEnvPath(L"FFMPEG_ROOT");
  const auto sdk_root_env = ExpandEnvPath(L"NV_RTX_VIDEO_SDK");
  const auto codec_root_env = ExpandEnvPath(L"NV_VIDEO_CODEC_INTERFACE");
  const auto flutter_root_env = ExpandEnvPath(L"FLUTTER_ROOT");
  const auto cmake_env = ExpandEnvPath(L"CMAKE_EXE");

  paths.ffmpeg_root =
      !ffmpeg_root_env.empty()
          ? ffmpeg_root_env
          : std::filesystem::path(L"C:\\Program Files (x86)\\ffmpeg-N-122395-g48c9c38684-win64-gpl-shared");
  paths.ffmpeg_bin_dir = paths.ffmpeg_root / "bin";
  if (!executable_dir.empty() && std::filesystem::exists(executable_dir / "ffmpeg.exe")) {
    paths.ffmpeg_bin_dir = executable_dir;
  }
  paths.ffmpeg_exe = FindOnPath(L"ffmpeg.exe");
  if (paths.ffmpeg_exe.empty()) {
    const auto bundled_ffmpeg = executable_dir / "ffmpeg.exe";
    if (!executable_dir.empty() && std::filesystem::exists(bundled_ffmpeg)) {
      paths.ffmpeg_exe = bundled_ffmpeg;
    } else {
      paths.ffmpeg_exe = paths.ffmpeg_bin_dir / "ffmpeg.exe";
    }
  }
  paths.ffprobe_exe = FindOnPath(L"ffprobe.exe");
  if (paths.ffprobe_exe.empty()) {
    const auto bundled_ffprobe = executable_dir / "ffprobe.exe";
    if (!executable_dir.empty() && std::filesystem::exists(bundled_ffprobe)) {
      paths.ffprobe_exe = bundled_ffprobe;
    } else {
      paths.ffprobe_exe = paths.ffmpeg_bin_dir / "ffprobe.exe";
    }
  }

  paths.flutter_root =
      !flutter_root_env.empty() ? flutter_root_env : std::filesystem::path(L"C:\\Users\\21826\\develop\\flutter");
  paths.flutter_exe = paths.flutter_root / "bin" / "flutter.bat";

  paths.cmake_exe =
      !cmake_env.empty()
          ? cmake_env
          : std::filesystem::path(
                L"C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe");

  paths.nvidia_sdk_root = !sdk_root_env.empty()
                              ? sdk_root_env
                              : std::filesystem::current_path() / "RTX_Video_SDK_v1.1.0";
  if (!std::filesystem::exists(paths.nvidia_sdk_root)) {
    paths.nvidia_sdk_root = std::filesystem::current_path().parent_path().parent_path() / "RTX_Video_SDK_v1.1.0";
  }
  paths.nvidia_sdk_runtime_dir = paths.nvidia_sdk_root / "bin" / "Windows" / "x64" / "rel";
  if (!executable_dir.empty() && std::filesystem::exists(executable_dir / "nvngx_vsr.dll")) {
    paths.nvidia_sdk_runtime_dir = executable_dir;
  }

  paths.video_codec_interface_root =
      !codec_root_env.empty() ? codec_root_env : std::filesystem::current_path() / "Video_Codec_Interface_13.0.37";
  if (!std::filesystem::exists(paths.video_codec_interface_root)) {
    paths.video_codec_interface_root =
        std::filesystem::current_path().parent_path().parent_path() / "Video_Codec_Interface_13.0.37";
  }

  return paths;
}

Status EnsureProcessRuntimePath(const RuntimePaths& paths) {
  if (!paths.ffmpeg_bin_dir.empty() && std::filesystem::exists(paths.ffmpeg_bin_dir)) {
    PrependPathIfMissing(paths.ffmpeg_bin_dir);
  }
  if (!paths.nvidia_sdk_runtime_dir.empty() && std::filesystem::exists(paths.nvidia_sdk_runtime_dir)) {
    PrependPathIfMissing(paths.nvidia_sdk_runtime_dir);
  }
  return Status::Ok();
}

}  // namespace rtx
