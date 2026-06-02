#include "platform/capabilities.h"

#include <filesystem>

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
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL feature_level = {};
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    const HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
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

    return SUCCEEDED(result);
#else
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

    snapshot.nvenc_h264_available = snapshot.d3d11_available;
    snapshot.nvenc_hevc_main10_available = snapshot.d3d11_available;
    snapshot.messages.push_back(
        "NVENC capability flags currently mirror D3D11 availability; true encoder probing will be added later.");

    return snapshot;
}

} // namespace vsr
