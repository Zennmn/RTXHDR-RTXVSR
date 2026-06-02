#include "platform/capabilities.h"

#include <filesystem>

#if defined(VSR_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

#ifdef _WIN32
#include <d3d11.h>
#include <windows.h>
#endif

namespace vsr {

namespace {

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::filesystem::path process_directory() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            break;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#endif

    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (!error) {
        return current;
    }
    return {};
}

bool detect_d3d11_available() {
#ifdef _WIN32
    const auto try_create_device = [](const D3D_FEATURE_LEVEL* feature_levels, UINT feature_level_count) {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        D3D_FEATURE_LEVEL feature_level = {};

        const HRESULT result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            feature_levels,
            feature_level_count,
            D3D11_SDK_VERSION,
            &device,
            &feature_level,
            &context);

        if (context != nullptr) {
            context->Release();
        }
        if (device != nullptr) {
            device->Release();
        }

        return result;
    };

    const D3D_FEATURE_LEVEL feature_levels_with_11_1[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT result =
        try_create_device(feature_levels_with_11_1, static_cast<UINT>(std::size(feature_levels_with_11_1)));
    if (result == E_INVALIDARG) {
        result = try_create_device(feature_levels, static_cast<UINT>(std::size(feature_levels)));
    }

    return SUCCEEDED(result);
#else
    return false;
#endif
}

bool ffmpeg_encoder_available(const char* name) {
#if defined(VSR_ENABLE_FFMPEG)
    return avcodec_find_encoder_by_name(name) != nullptr;
#else
    (void)name;
    return false;
#endif
}

} // namespace

nlohmann::json capability_snapshot_to_json(const CapabilitySnapshot& snapshot) {
    return {
        {"d3d11Available", snapshot.d3d11_available},
        {"rtxSdkFound", snapshot.rtx_sdk_found},
        {"vsrAvailable", snapshot.vsr_available},
        {"truehdrAvailable", snapshot.truehdr_available},
        {"nvencH264Available", snapshot.nvenc_h264_available},
        {"nvencHevcMain10Available", snapshot.nvenc_hevc_main10_available},
        {"messages", snapshot.messages},
    };
}

CapabilitySnapshot detect_capabilities() {
    CapabilitySnapshot snapshot;
    snapshot.d3d11_available = detect_d3d11_available();
    if (!snapshot.d3d11_available) {
        snapshot.messages.push_back("D3D11 hardware device creation is unavailable.");
    }

    const auto directory = process_directory();
    const auto vsr_dll = directory / "nvngx_vsr.dll";
    const auto truehdr_dll = directory / "nvngx_truehdr.dll";
    const bool vsr_dll_found = file_exists(vsr_dll);
    const bool truehdr_dll_found = file_exists(truehdr_dll);

    if (!vsr_dll_found) {
        snapshot.messages.push_back("nvngx_vsr.dll was not found beside the backend executable.");
    }
    if (!truehdr_dll_found) {
        snapshot.messages.push_back("nvngx_truehdr.dll was not found beside the backend executable.");
    }

    snapshot.rtx_sdk_found = vsr_dll_found && truehdr_dll_found;
    snapshot.vsr_available = snapshot.d3d11_available && vsr_dll_found;
    snapshot.truehdr_available = snapshot.d3d11_available && truehdr_dll_found;

    const bool h264_nvenc_found = ffmpeg_encoder_available("h264_nvenc");
    const bool hevc_nvenc_found = ffmpeg_encoder_available("hevc_nvenc");
    snapshot.nvenc_h264_available = snapshot.d3d11_available && h264_nvenc_found;
    snapshot.nvenc_hevc_main10_available = snapshot.d3d11_available && hevc_nvenc_found;
#if !defined(VSR_ENABLE_FFMPEG)
    snapshot.messages.push_back("FFmpeg support is not enabled in this backend build.");
#else
    if (!h264_nvenc_found) {
        snapshot.messages.push_back("FFmpeg h264_nvenc encoder was not found.");
    }
    if (!hevc_nvenc_found) {
        snapshot.messages.push_back("FFmpeg hevc_nvenc encoder was not found.");
    }
#endif

    return snapshot;
}

} // namespace vsr
