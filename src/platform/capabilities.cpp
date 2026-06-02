#include "platform/capabilities.h"

#include <filesystem>

#if defined(VSR_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}
#endif

#ifdef _WIN32
#include <d3d11.h>
#include <windows.h>
#endif

#if defined(VSR_ENABLE_FFMPEG) && defined(_WIN32)
extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}
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

#if defined(VSR_ENABLE_FFMPEG) && defined(_WIN32)
bool detect_nvenc_d3d11_available_uncached(const char* encoder_name, AVPixelFormat software_format, bool hevc_main10) {
    const AVCodec* encoder = avcodec_find_encoder_by_name(encoder_name);
    if (encoder == nullptr) {
        return false;
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL feature_level = {};
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT d3d_result = D3D11CreateDevice(
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
    if (d3d_result == E_INVALIDARG) {
        d3d_result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            feature_levels + 1,
            static_cast<UINT>(std::size(feature_levels) - 1),
            D3D11_SDK_VERSION,
            &device,
            &feature_level,
            &context);
    }
    if (FAILED(d3d_result)) {
        if (context != nullptr) {
            context->Release();
        }
        if (device != nullptr) {
            device->Release();
        }
        return false;
    }

    AVBufferRef* hw_device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (hw_device_ref == nullptr) {
        context->Release();
        device->Release();
        return false;
    }
    AVHWDeviceContext* hw_device = reinterpret_cast<AVHWDeviceContext*>(hw_device_ref->data);
    auto* d3d11_device_context = reinterpret_cast<AVD3D11VADeviceContext*>(hw_device->hwctx);
    d3d11_device_context->device = device;
    d3d11_device_context->device_context = context;
    device->AddRef();
    context->AddRef();

    bool available = false;
    int result = av_hwdevice_ctx_init(hw_device_ref);
    if (result >= 0) {
        AVBufferRef* frames_ref = av_hwframe_ctx_alloc(hw_device_ref);
        if (frames_ref != nullptr) {
            auto* frames = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
            frames->format = AV_PIX_FMT_D3D11;
            frames->sw_format = software_format;
            frames->width = 1280;
            frames->height = 720;
            frames->initial_pool_size = 1;

            auto* d3d11_frames = reinterpret_cast<AVD3D11VAFramesContext*>(frames->hwctx);
            d3d11_frames->BindFlags =
                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

            result = av_hwframe_ctx_init(frames_ref);
            if (result >= 0) {
                AVCodecContext* encoder_context = avcodec_alloc_context3(encoder);
                if (encoder_context != nullptr) {
                    encoder_context->width = 1280;
                    encoder_context->height = 720;
                    encoder_context->time_base = AVRational{1, 30};
                    encoder_context->framerate = AVRational{30, 1};
                    encoder_context->pix_fmt = AV_PIX_FMT_D3D11;
                    encoder_context->sw_pix_fmt = software_format;
                    if (hevc_main10) {
                        encoder_context->profile = 2;
                        encoder_context->color_primaries = AVCOL_PRI_BT2020;
                        encoder_context->color_trc = AVCOL_TRC_SMPTE2084;
                        encoder_context->colorspace = AVCOL_SPC_BT2020_NCL;
                        encoder_context->color_range = AVCOL_RANGE_MPEG;
                    }
                    encoder_context->hw_frames_ctx = av_buffer_ref(frames_ref);
                    if (hevc_main10) {
                        av_opt_set(encoder_context->priv_data, "profile", "main10", 0);
                        av_opt_set(encoder_context->priv_data, "tier", "main", 0);
                    }
                    av_opt_set(encoder_context->priv_data, "preset", "p4", 0);
                    available = encoder_context->hw_frames_ctx != nullptr &&
                        avcodec_open2(encoder_context, encoder, nullptr) >= 0;
                    avcodec_free_context(&encoder_context);
                }
            }
            av_buffer_unref(&frames_ref);
        }
    }

    av_buffer_unref(&hw_device_ref);
    context->Release();
    device->Release();
    return available;
}
#endif

bool detect_h264_nvenc_d3d11_available() {
#if defined(VSR_ENABLE_FFMPEG) && defined(_WIN32)
    static const bool available = detect_nvenc_d3d11_available_uncached("h264_nvenc", AV_PIX_FMT_BGRA, false);
    return available;
#else
    return false;
#endif
}

bool detect_hevc_nvenc_main10_d3d11_available() {
#if defined(VSR_ENABLE_FFMPEG) && defined(_WIN32)
    static const bool available = detect_nvenc_d3d11_available_uncached("hevc_nvenc", AV_PIX_FMT_X2BGR10, true);
    return available;
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

    const bool h264_nvenc_found = ffmpeg_encoder_available("h264_nvenc");
    const bool hevc_nvenc_found = ffmpeg_encoder_available("hevc_nvenc");
    const bool h264_d3d11_found =
        snapshot.d3d11_available && h264_nvenc_found && detect_h264_nvenc_d3d11_available();
    const bool hevc_main10_d3d11_found =
        snapshot.d3d11_available && hevc_nvenc_found && detect_hevc_nvenc_main10_d3d11_available();
    snapshot.nvenc_h264_available = h264_d3d11_found;
    snapshot.nvenc_hevc_main10_available = hevc_main10_d3d11_found;
#if !defined(VSR_ENABLE_FFMPEG)
    snapshot.messages.push_back("FFmpeg support is not enabled in this backend build.");
#else
    if (!h264_nvenc_found) {
        snapshot.messages.push_back("FFmpeg h264_nvenc encoder was not found.");
    } else if (!h264_d3d11_found) {
        snapshot.messages.push_back("FFmpeg h264_nvenc D3D11 encode path is unavailable.");
    }
    if (!hevc_nvenc_found) {
        snapshot.messages.push_back("FFmpeg hevc_nvenc encoder was not found.");
    } else if (!hevc_main10_d3d11_found) {
        snapshot.messages.push_back("FFmpeg hevc_nvenc Main10 D3D11 encode path is unavailable.");
    }
#endif

    return snapshot;
}

} // namespace vsr
