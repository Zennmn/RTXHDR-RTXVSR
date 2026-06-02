#include "video/rtx/rtx_dx11_processor.h"

#if !defined(_WIN32)
#error "RtxDx11Processor requires Windows and Direct3D 11."
#endif

#include <rtx_video_api.h>

#include <mutex>
#include <string>

namespace vsr {
namespace {

std::mutex& sdk_mutex() {
    static std::mutex mutex;
    return mutex;
}

bool& sdk_active() {
    static bool active = false;
    return active;
}

Result<void> validate_processing_settings(const ProcessingSettings& settings) {
    if (!settings.vsr.enabled && !settings.hdr.enabled) {
        return Result<void>::Fail({"rtx_processing_required", "Enable VSR, HDR, or both.", ""});
    }
    if (settings.vsr.enabled && (settings.vsr.quality < 1 || settings.vsr.quality > 4)) {
        return Result<void>::Fail({"invalid_vsr_quality", "VSR quality must be between 1 and 4.", std::to_string(settings.vsr.quality)});
    }
    if (settings.vsr.enabled && (settings.vsr.scale < 1.0 || settings.vsr.scale > 4.0)) {
        return Result<void>::Fail({"invalid_vsr_scale", "VSR scale must be between 1.0 and 4.0.", std::to_string(settings.vsr.scale)});
    }
    if (settings.hdr.enabled && (settings.hdr.contrast < 0 || settings.hdr.contrast > 200)) {
        return Result<void>::Fail({"invalid_hdr_contrast", "HDR contrast must be between 0 and 200.", std::to_string(settings.hdr.contrast)});
    }
    if (settings.hdr.enabled && (settings.hdr.saturation < 0 || settings.hdr.saturation > 200)) {
        return Result<void>::Fail({"invalid_hdr_saturation", "HDR saturation must be between 0 and 200.", std::to_string(settings.hdr.saturation)});
    }
    if (settings.hdr.enabled && (settings.hdr.middle_gray < 10 || settings.hdr.middle_gray > 100)) {
        return Result<void>::Fail({"invalid_hdr_middle_gray", "HDR middle gray must be between 10 and 100.", std::to_string(settings.hdr.middle_gray)});
    }
    if (settings.hdr.enabled && (settings.hdr.max_luminance < 400 || settings.hdr.max_luminance > 2000)) {
        return Result<void>::Fail({"invalid_hdr_max_luminance", "HDR max luminance must be between 400 and 2000.", std::to_string(settings.hdr.max_luminance)});
    }
    return Result<void>::Ok();
}

API_BOOL api_bool(bool enabled) {
    return enabled ? API_BOOL_SUCCESS : API_BOOL_FAIL;
}

API_RECT full_rect(std::uint32_t width, std::uint32_t height) {
    return API_RECT{0, 0, width, height};
}

API_VSR_Setting to_api_vsr_setting(const VsrSettings& settings) {
    return API_VSR_Setting{static_cast<std::uint32_t>(settings.quality)};
}

API_THDR_Setting to_api_thdr_setting(const HdrSettings& settings) {
    return API_THDR_Setting{
        static_cast<std::uint32_t>(settings.contrast),
        static_cast<std::uint32_t>(settings.saturation),
        static_cast<std::uint32_t>(settings.middle_gray),
        static_cast<std::uint32_t>(settings.max_luminance),
    };
}

std::string mode_details(bool initialized_vsr, bool initialized_hdr, const ProcessingSettings& requested) {
    return "initialized_vsr=" + std::to_string(initialized_vsr) +
        ", initialized_hdr=" + std::to_string(initialized_hdr) +
        ", requested_vsr=" + std::to_string(requested.vsr.enabled) +
        ", requested_hdr=" + std::to_string(requested.hdr.enabled);
}

} // namespace

RtxDx11Processor::~RtxDx11Processor() {
    shutdown();
}

Result<void> RtxDx11Processor::initialize(ID3D11Device* device, const ProcessingSettings& settings) {
    std::lock_guard lock(sdk_mutex());

    if (initialized_) {
        return Result<void>::Fail({"rtx_already_initialized", "RTX DX11 processor is already initialized.", ""});
    }
    if (sdk_active()) {
        return Result<void>::Fail({"rtx_processor_active", "Another RTX DX11 processor is already initialized.", ""});
    }
    if (device == nullptr) {
        return Result<void>::Fail({"rtx_device_required", "A D3D11 device is required to initialize RTX processing.", ""});
    }

    const auto valid = validate_processing_settings(settings);
    if (!valid.ok()) {
        return Result<void>::Fail(valid.error());
    }

    const API_BOOL created = rtx_video_api_dx11_create(device, api_bool(settings.hdr.enabled), api_bool(settings.vsr.enabled));
    if (created != API_BOOL_SUCCESS) {
        rtx_video_api_dx11_shutdown();
        return Result<void>::Fail({"rtx_initialize_failed", "RTX Video DX11 API initialization failed.", ""});
    }

    initialized_ = true;
    initialized_vsr_enabled_ = settings.vsr.enabled;
    initialized_hdr_enabled_ = settings.hdr.enabled;
    sdk_active() = true;
    return Result<void>::Ok();
}

Result<void> RtxDx11Processor::process(const RtxDx11Frame& frame, const ProcessingSettings& settings) {
    std::lock_guard lock(sdk_mutex());

    if (!initialized_) {
        return Result<void>::Fail({"rtx_not_initialized", "RTX DX11 processor must be initialized before processing.", ""});
    }
    if (frame.input == nullptr) {
        return Result<void>::Fail({"rtx_input_texture_required", "An input D3D11 texture is required for RTX processing.", ""});
    }
    if (frame.output == nullptr) {
        return Result<void>::Fail({"rtx_output_texture_required", "An output D3D11 texture is required for RTX processing.", ""});
    }
    if (frame.input_width == 0 || frame.input_height == 0 || frame.output_width == 0 || frame.output_height == 0) {
        return Result<void>::Fail({"rtx_invalid_frame_dimensions", "Input and output frame dimensions must be non-zero.", ""});
    }

    const auto valid = validate_processing_settings(settings);
    if (!valid.ok()) {
        return Result<void>::Fail(valid.error());
    }
    if (settings.vsr.enabled != initialized_vsr_enabled_ || settings.hdr.enabled != initialized_hdr_enabled_) {
        return Result<void>::Fail({
            "rtx_processing_mode_mismatch",
            "Processing mode must match the VSR/HDR features created during RTX initialization.",
            mode_details(initialized_vsr_enabled_, initialized_hdr_enabled_, settings)
        });
    }

    API_VSR_Setting vsr_setting = to_api_vsr_setting(settings.vsr);
    API_THDR_Setting thdr_setting = to_api_thdr_setting(settings.hdr);
    API_VSR_Setting* vsr_setting_ptr = settings.vsr.enabled ? &vsr_setting : nullptr;
    API_THDR_Setting* thdr_setting_ptr = settings.hdr.enabled ? &thdr_setting : nullptr;

    const API_BOOL evaluated = rtx_video_api_dx11_evaluate(
        frame.input,
        frame.output,
        full_rect(frame.input_width, frame.input_height),
        full_rect(frame.output_width, frame.output_height),
        vsr_setting_ptr,
        thdr_setting_ptr);
    if (evaluated != API_BOOL_SUCCESS) {
        return Result<void>::Fail({"rtx_process_failed", "RTX Video DX11 API processing failed.", ""});
    }

    return Result<void>::Ok();
}

void RtxDx11Processor::shutdown() {
    std::lock_guard lock(sdk_mutex());

    if (!initialized_) {
        return;
    }

    rtx_video_api_dx11_shutdown();
    initialized_ = false;
    initialized_vsr_enabled_ = false;
    initialized_hdr_enabled_ = false;
    sdk_active() = false;
}

} // namespace vsr
