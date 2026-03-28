#include "rtx/platform/system_probe.h"

#include <windows.h>

#include <cstdio>
#include <sstream>
#include <string>

#include <cuda.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs_truehdr.h>
#include <nvsdk_ngx_defs_vsr.h>

#include "rtx/video_io/ffmpeg_probe.h"
#include "rtx/video_io/ffmpeg_runtime.h"

namespace rtx {

namespace {

std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
    value.pop_back();
  }
  while (!value.empty() && value.front() == ' ') {
    value.erase(value.begin());
  }
  return value;
}

std::string RunPipeCommand(const char* command) {
  std::string output;
  FILE* pipe = _popen(command, "r");
  if (pipe == nullptr) {
    return output;
  }

  char buffer[256] = {};
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  _pclose(pipe);
  return Trim(output);
}

std::string GetWindowsVersionString() {
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll == nullptr) {
    return "Windows";
  }

  using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  const auto rtl_get_version =
      reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
  if (rtl_get_version == nullptr) {
    return "Windows";
  }

  RTL_OSVERSIONINFOW version_info{};
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (rtl_get_version(&version_info) != 0) {
    return "Windows";
  }

  std::ostringstream stream;
  stream << "Windows " << version_info.dwMajorVersion << "." << version_info.dwMinorVersion
         << " build " << version_info.dwBuildNumber;
  return stream.str();
}

std::string FormatCudaVersion(int version) {
  if (version <= 0) {
    return {};
  }

  const int major = version / 1000;
  const int minor = (version % 1000) / 10;
  std::ostringstream stream;
  stream << major << "." << minor;
  return stream.str();
}

Status ProbeNgxCapabilities(const RuntimePaths& runtime_paths,
                            bool& vsr_available,
                            bool& truehdr_available) {
  vsr_available = false;
  truehdr_available = false;

  const auto ensure_status = EnsureProcessRuntimePath(runtime_paths);
  if (!ensure_status.ok()) {
    return ensure_status;
  }

  if (cuInit(0) != CUDA_SUCCESS) {
    return Status::Unavailable("cuInit failed during capability probe.");
  }

  NVSDK_NGX_Result init_result = NVSDK_NGX_CUDA_Init(0, L".");
  if (NVSDK_NGX_FAILED(init_result)) {
    return Status::Unavailable("NVSDK_NGX_CUDA_Init failed during capability probe.");
  }

  NVSDK_NGX_Parameter* parameters = nullptr;
  const NVSDK_NGX_Result param_result = NVSDK_NGX_CUDA_GetCapabilityParameters(&parameters);
  if (NVSDK_NGX_FAILED(param_result) || parameters == nullptr) {
    NVSDK_NGX_CUDA_Shutdown();
    return Status::Unavailable("Unable to query NGX capability parameters.");
  }

  int vsr = 0;
  int hdr = 0;
  parameters->Get(NVSDK_NGX_Parameter_VSR_Available, &vsr);
  parameters->Get(NVSDK_NGX_Parameter_TrueHDR_Available, &hdr);
  vsr_available = vsr != 0;
  truehdr_available = hdr != 0;

  NVSDK_NGX_CUDA_DestroyParameters(parameters);
  NVSDK_NGX_CUDA_Shutdown();
  return Status::Ok();
}

}  // namespace

Status ProbeCurrentSystem(Logger& logger, SystemProbe& probe) {
  probe = SystemProbe{};
  probe.os_version = GetWindowsVersionString();
  probe.runtime_paths = ResolveRuntimePaths();

  FfmpegProbe ffmpeg_probe(logger);
  probe.h264_hw_decode = ffmpeg_probe.CanDecodeWithHardware("h264");
  probe.hevc_hw_decode = ffmpeg_probe.CanDecodeWithHardware("hevc");
  probe.av1_hw_decode = ffmpeg_probe.CanDecodeWithHardware("av1");
  probe.h264_hw_encode = ffmpeg_probe.CanEncodeWithHardware("h264");
  probe.hevc_hw_encode = ffmpeg_probe.CanEncodeWithHardware("hevc");
  probe.av1_hw_encode = ffmpeg_probe.CanEncodeWithHardware("av1");
  probe.hdr_export_available = probe.hevc_hw_encode || probe.av1_hw_encode;
  probe.supported_pixel_formats = "bgra,x2bgr10le,p010le";

  if (cuInit(0) == CUDA_SUCCESS) {
    int driver_version = 0;
    if (cuDriverGetVersion(&driver_version) == CUDA_SUCCESS) {
      probe.cuda_version = FormatCudaVersion(driver_version);
    }

    CUdevice device = 0;
    if (cuDeviceGet(&device, 0) == CUDA_SUCCESS) {
      char name[256] = {};
      if (cuDeviceGetName(name, sizeof(name), device) == CUDA_SUCCESS) {
        probe.gpu_name = name;
        probe.has_nvidia_gpu = true;
      }
    }
  }

  const std::string smi_line =
      RunPipeCommand("nvidia-smi --query-gpu=name,driver_version --format=csv,noheader");
  if (!smi_line.empty()) {
    const auto separator = smi_line.find(',');
    if (separator != std::string::npos) {
      probe.gpu_name = Trim(smi_line.substr(0, separator));
      probe.driver_version = Trim(smi_line.substr(separator + 1));
      probe.has_nvidia_gpu = true;
    }
  }

  bool vsr_available = false;
  bool hdr_available = false;
  const auto ngx_status = ProbeNgxCapabilities(probe.runtime_paths, vsr_available, hdr_available);
  if (ngx_status.ok()) {
    probe.ngx_vsr_available = vsr_available;
    probe.ngx_truehdr_available = hdr_available;
  } else {
    probe.notes = ngx_status.message();
  }

  logger.Debug("System probe complete for " + probe.gpu_name);
  return Status::Ok();
}

}  // namespace rtx

