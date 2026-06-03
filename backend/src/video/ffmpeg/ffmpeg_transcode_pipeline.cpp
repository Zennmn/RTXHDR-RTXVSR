#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include "platform/logging.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

#if defined(_WIN32)
#include <libavutil/hwcontext_d3d11va.h>
#endif

namespace vsr {
namespace {

struct InputFormatContextDeleter {
    void operator()(AVFormatContext* context) const noexcept {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

struct OutputFormatContextDeleter {
    void operator()(AVFormatContext* context) const noexcept {
        if (context == nullptr) {
            return;
        }
        if (context->pb != nullptr && context->oformat != nullptr && (context->oformat->flags & AVFMT_NOFILE) == 0) {
            avio_closep(&context->pb);
        }
        avformat_free_context(context);
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* context) const noexcept {
        if (context != nullptr) {
            avcodec_free_context(&context);
        }
    }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const noexcept {
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
    }
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const noexcept {
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};

struct BufferRefDeleter {
    void operator()(AVBufferRef* ref) const noexcept {
        if (ref != nullptr) {
            av_buffer_unref(&ref);
        }
    }
};

using InputFormatContextPtr = std::unique_ptr<AVFormatContext, InputFormatContextDeleter>;
using OutputFormatContextPtr = std::unique_ptr<AVFormatContext, OutputFormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using BufferRefPtr = std::unique_ptr<AVBufferRef, BufferRefDeleter>;

static Error ffmpeg_error(const char* code, const char* message, int ffmpeg_code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(ffmpeg_code, buffer, sizeof(buffer));
    return {code, message, buffer};
}

Result<void> set_required_encoder_option(AVCodecContext* context, const char* name, const char* value) {
    const int result = av_opt_set(context->priv_data, name, value, 0);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error("encoder_option_failed", "FFmpeg could not set a required NVENC option.", result));
    }
    return Result<void>::Ok();
}

Result<void> set_required_encoder_option_double(AVCodecContext* context, const char* name, double value) {
    const int result = av_opt_set_double(context->priv_data, name, value, 0);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error("encoder_option_failed", "FFmpeg could not set a required NVENC option.", result));
    }
    return Result<void>::Ok();
}

void set_optional_encoder_option_int(AVCodecContext* context, const char* name, std::int64_t value) {
    const int result = av_opt_set_int(context->priv_data, name, value, 0);
    if (result < 0) {
        log_info(std::string("Skipping unsupported NVENC option: ") + name);
    }
}

Result<void> configure_nvenc_quality(AVCodecContext* context) {
    for (const auto& option : {
             std::pair<const char*, const char*>{"preset", "p7"},
             std::pair<const char*, const char*>{"tune", "hq"},
             std::pair<const char*, const char*>{"rc", "vbr"},
         }) {
        const auto configured = set_required_encoder_option(context, option.first, option.second);
        if (!configured.ok()) {
            return configured;
        }
    }

    const auto cq = set_required_encoder_option_double(context, "cq", 18.0);
    if (!cq.ok()) {
        return cq;
    }

    set_optional_encoder_option_int(context, "spatial_aq", 1);
    set_optional_encoder_option_int(context, "temporal_aq", 1);
    set_optional_encoder_option_int(context, "aq-strength", 8);
    set_optional_encoder_option_int(context, "rc-lookahead", 32);
    set_optional_encoder_option_int(context, "multipass", 2);
    return Result<void>::Ok();
}

std::int64_t source_video_bit_rate(const AVFormatContext* format, const AVStream* stream, const AVCodecContext* decoder) {
    if (stream != nullptr && stream->codecpar != nullptr && stream->codecpar->bit_rate > 0) {
        return stream->codecpar->bit_rate;
    }
    if (decoder != nullptr && decoder->bit_rate > 0) {
        return decoder->bit_rate;
    }
    if (format != nullptr && format->bit_rate > 0) {
        return format->bit_rate;
    }
    return 0;
}

static double progress_from_frames(std::int64_t frames_done, std::int64_t frames_total) {
    return ffmpeg_progress_from_frames(frames_done, frames_total);
}

int find_primary_video_stream(const AVFormatContext* format) {
    for (unsigned int index = 0; index < format->nb_streams; ++index) {
        const AVStream* stream = format->streams[index];
        if (stream != nullptr && stream->codecpar != nullptr && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool valid_rational(AVRational value) {
    return value.num > 0 && value.den > 0;
}

bool mp4_audio_copy_compatible(AVCodecID codec_id) {
    switch (codec_id) {
    case AV_CODEC_ID_AAC:
    case AV_CODEC_ID_ALAC:
    case AV_CODEC_ID_MP3:
    case AV_CODEC_ID_AC3:
    case AV_CODEC_ID_EAC3:
        return true;
    default:
        return false;
    }
}

bool mp4_subtitle_copy_compatible(AVCodecID codec_id) {
    return codec_id == AV_CODEC_ID_MOV_TEXT;
}

std::string codec_details(const AVStream* stream) {
    if (stream == nullptr || stream->codecpar == nullptr) {
        return "unknown stream";
    }
    const char* codec_name = avcodec_get_name(stream->codecpar->codec_id);
    std::ostringstream details;
    details << "stream=" << stream->index << ", codec=" << (codec_name != nullptr ? codec_name : "unknown");
    return details.str();
}

std::int64_t estimate_total_frames(const AVFormatContext* format, const AVStream* stream, AVRational frame_rate) {
    if (stream->nb_frames > 0) {
        return stream->nb_frames;
    }
    if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0 && valid_rational(stream->time_base) &&
        valid_rational(frame_rate)) {
        const double seconds = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
        return static_cast<std::int64_t>(std::llround(seconds * av_q2d(frame_rate)));
    }
    if (format->duration != AV_NOPTS_VALUE && format->duration > 0 && valid_rational(frame_rate)) {
        const double seconds = static_cast<double>(format->duration) / static_cast<double>(AV_TIME_BASE);
        return static_cast<std::int64_t>(std::llround(seconds * av_q2d(frame_rate)));
    }
    return 0;
}

int even_scaled_dimension(int value, double scale) {
    const double scaled = static_cast<double>(value) * scale;
    auto rounded = static_cast<long long>(std::llround(scaled));
    rounded = std::max<long long>(2, rounded);
    if ((rounded % 2) != 0) {
        ++rounded;
    }
    return static_cast<int>(std::min<long long>(rounded, std::numeric_limits<int>::max()));
}

AVPixelFormat choose_d3d11_format(AVCodecContext*, const AVPixelFormat* formats) {
    for (const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == AV_PIX_FMT_D3D11) {
            return *format;
        }
    }
    return AV_PIX_FMT_NONE;
}

#if defined(_WIN32)

Error hresult_error(const char* code, const char* message, HRESULT result) {
    std::ostringstream details;
    details << "HRESULT 0x" << std::uppercase << std::hex << static_cast<unsigned long>(result);
    return {code, message, details.str()};
}

struct D3d11DevicePair {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

Result<D3d11DevicePair> create_d3d11_device() {
    D3d11DevicePair pair;
    D3D_FEATURE_LEVEL created_feature_level = {};
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

    const UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels_with_11_1,
        static_cast<UINT>(std::size(feature_levels_with_11_1)),
        D3D11_SDK_VERSION,
        pair.device.ReleaseAndGetAddressOf(),
        &created_feature_level,
        pair.context.ReleaseAndGetAddressOf());
    if (result == E_INVALIDARG) {
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            feature_levels,
            static_cast<UINT>(std::size(feature_levels)),
            D3D11_SDK_VERSION,
            pair.device.ReleaseAndGetAddressOf(),
            &created_feature_level,
            pair.context.ReleaseAndGetAddressOf());
    }
    if (FAILED(result)) {
        return Result<D3d11DevicePair>::Fail(hresult_error(
            "d3d11_device_create_failed",
            "A D3D11 hardware video device could not be created.",
            result));
    }

    return Result<D3d11DevicePair>::Ok(std::move(pair));
}

Result<BufferRefPtr> create_ffmpeg_d3d11_device(ID3D11Device* device, ID3D11DeviceContext* context) {
    AVBufferRef* raw_device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (raw_device_ref == nullptr) {
        return Result<BufferRefPtr>::Fail({"d3d11va_device_alloc_failed", "FFmpeg could not allocate a D3D11VA device context.", ""});
    }
    BufferRefPtr device_ref(raw_device_ref);

    auto* hw_device = reinterpret_cast<AVHWDeviceContext*>(device_ref->data);
    auto* d3d11 = reinterpret_cast<AVD3D11VADeviceContext*>(hw_device->hwctx);
    d3d11->device = device;
    d3d11->device->AddRef();
    d3d11->device_context = context;
    d3d11->device_context->AddRef();

    const int result = av_hwdevice_ctx_init(device_ref.get());
    if (result < 0) {
        return Result<BufferRefPtr>::Fail(ffmpeg_error(
            "d3d11va_device_init_failed",
            "FFmpeg could not initialize the D3D11VA device context.",
            result));
    }

    return Result<BufferRefPtr>::Ok(std::move(device_ref));
}

DXGI_FORMAT dxgi_format_for_encoder_surface(AVPixelFormat format) {
    switch (format) {
    case AV_PIX_FMT_BGRA:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case AV_PIX_FMT_X2BGR10:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_COLOR_SPACE_TYPE d3d11_input_color_space(const AVCodecContext* decoder) {
    const bool full_range = decoder != nullptr && decoder->color_range == AVCOL_RANGE_JPEG;
    const bool bt2020 = decoder != nullptr &&
        (decoder->colorspace == AVCOL_SPC_BT2020_NCL || decoder->color_primaries == AVCOL_PRI_BT2020);
    const bool pq = decoder != nullptr && decoder->color_trc == AVCOL_TRC_SMPTE2084;

    if (bt2020 && pq) {
        return DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
    }
    if (bt2020) {
        return full_range ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
    }
    return full_range ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
}

DXGI_COLOR_SPACE_TYPE d3d11_rtx_input_color_space(const AVCodecContext* decoder) {
    const bool bt2020 = decoder != nullptr &&
        (decoder->colorspace == AVCOL_SPC_BT2020_NCL || decoder->color_primaries == AVCOL_PRI_BT2020);
    return bt2020 ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
}

Result<void> create_texture(
    ID3D11Device* device,
    int width,
    int height,
    DXGI_FORMAT format,
    UINT bind_flags,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bind_flags;

    const HRESULT result = device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_texture_create_failed",
            "A D3D11 texture required by the hardware pipeline could not be created.",
            result));
    }
    return Result<void>::Ok();
}

Result<void> convert_texture_with_video_processor(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* input,
    UINT input_slice,
    ID3D11Texture2D* output,
    UINT output_slice,
    int input_width,
    int input_height,
    int output_width,
    int output_height,
    DXGI_COLOR_SPACE_TYPE input_color_space,
    DXGI_COLOR_SPACE_TYPE output_color_space) {
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
    HRESULT result = device->QueryInterface(IID_PPV_ARGS(video_device.GetAddressOf()));
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_device_required",
            "The D3D11 device does not expose video processing support.",
            result));
    }

    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
    result = context->QueryInterface(IID_PPV_ARGS(video_context.GetAddressOf()));
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_context_required",
            "The D3D11 immediate context does not expose video processing support.",
            result));
    }

    D3D11_TEXTURE2D_DESC input_desc = {};
    D3D11_TEXTURE2D_DESC output_desc = {};
    input->GetDesc(&input_desc);
    output->GetDesc(&output_desc);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputWidth = static_cast<UINT>(input_width);
    content_desc.InputHeight = static_cast<UINT>(input_height);
    content_desc.OutputWidth = static_cast<UINT>(output_width);
    content_desc.OutputHeight = static_cast<UINT>(output_height);
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> enumerator;
    result = video_device->CreateVideoProcessorEnumerator(&content_desc, enumerator.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_enumerator_failed",
            "D3D11 video processor enumeration failed.",
            result));
    }

    UINT format_flags = 0;
    result = enumerator->CheckVideoProcessorFormat(input_desc.Format, &format_flags);
    if (FAILED(result) || (format_flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) == 0) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_input_format_unsupported",
            "The D3D11 video processor cannot use the decoded frame format as input.",
            FAILED(result) ? result : E_INVALIDARG));
    }

    format_flags = 0;
    result = enumerator->CheckVideoProcessorFormat(output_desc.Format, &format_flags);
    if (FAILED(result) || (format_flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) == 0) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_output_format_unsupported",
            "The D3D11 video processor cannot write the RTX texture format.",
            FAILED(result) ? result : E_INVALIDARG));
    }

    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> processor;
    result = video_device->CreateVideoProcessor(enumerator.Get(), 0, processor.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_create_failed",
            "D3D11 video processor creation failed.",
            result));
    }

    RECT output_rect = {0, 0, static_cast<LONG>(output_width), static_cast<LONG>(output_height)};
    video_context->VideoProcessorSetStreamFrameFormat(processor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    video_context->VideoProcessorSetStreamOutputRate(
        processor.Get(),
        0,
        D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL,
        TRUE,
        nullptr);
    video_context->VideoProcessorSetStreamSourceRect(processor.Get(), 0, FALSE, nullptr);
    video_context->VideoProcessorSetStreamDestRect(processor.Get(), 0, TRUE, &output_rect);
    video_context->VideoProcessorSetOutputTargetRect(processor.Get(), TRUE, &output_rect);
    Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context1;
    if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(video_context1.GetAddressOf())))) {
        video_context1->VideoProcessorSetStreamColorSpace1(processor.Get(), 0, input_color_space);
        video_context1->VideoProcessorSetOutputColorSpace1(processor.Get(), output_color_space);
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {};
    if (output_desc.ArraySize > 1) {
        output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
        output_view_desc.Texture2DArray.MipSlice = 0;
        output_view_desc.Texture2DArray.FirstArraySlice = output_slice;
        output_view_desc.Texture2DArray.ArraySize = 1;
    } else {
        output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        output_view_desc.Texture2D.MipSlice = 0;
    }

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view;
    result = video_device->CreateVideoProcessorOutputView(
        output,
        enumerator.Get(),
        &output_view_desc,
        output_view.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_output_view_failed",
            "D3D11 video processor output view creation failed.",
            result));
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> input_resource;
    result = input->QueryInterface(IID_PPV_ARGS(input_resource.GetAddressOf()));
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_input_resource_failed",
            "The decoded D3D11 texture could not be used as a video processor resource.",
            result));
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {};
    input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_view_desc.Texture2D.MipSlice = 0;
    input_view_desc.Texture2D.ArraySlice = input_slice;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    result = video_device->CreateVideoProcessorInputView(
        input_resource.Get(),
        enumerator.Get(),
        &input_view_desc,
        input_view.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_input_view_failed",
            "D3D11 video processor input view creation failed.",
            result));
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = input_view.Get();

    result = video_context->VideoProcessorBlt(processor.Get(), output_view.Get(), 0, 1, &stream);
    if (FAILED(result)) {
        return Result<void>::Fail(hresult_error(
            "d3d11_video_processor_blt_failed",
            "D3D11 video processor conversion failed.",
            result));
    }

    context->Flush();
    return Result<void>::Ok();
}

Result<BufferRefPtr> create_encoder_frames_context(
    AVBufferRef* hw_device_ref,
    AVPixelFormat sw_format,
    int width,
    int height) {
    AVBufferRef* raw_frames_ref = av_hwframe_ctx_alloc(hw_device_ref);
    if (raw_frames_ref == nullptr) {
        return Result<BufferRefPtr>::Fail({"encoder_frames_alloc_failed", "FFmpeg could not allocate encoder D3D11 frames.", ""});
    }
    BufferRefPtr frames_ref(raw_frames_ref);

    auto* frames = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
    frames->format = AV_PIX_FMT_D3D11;
    frames->sw_format = sw_format;
    frames->width = width;
    frames->height = height;
    frames->initial_pool_size = 0;

    auto* d3d11_frames = reinterpret_cast<AVD3D11VAFramesContext*>(frames->hwctx);
    d3d11_frames->BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    const int result = av_hwframe_ctx_init(frames_ref.get());
    if (result < 0) {
        return Result<BufferRefPtr>::Fail(ffmpeg_error(
            "encoder_frames_init_failed",
            "FFmpeg could not initialize encoder D3D11 frames.",
            result));
    }

    return Result<BufferRefPtr>::Ok(std::move(frames_ref));
}

Result<void> drain_encoder(AVCodecContext* encoder, AVFormatContext* output, AVStream* stream) {
    PacketPtr packet(av_packet_alloc());
    if (!packet) {
        return Result<void>::Fail({"packet_alloc_failed", "FFmpeg could not allocate an encoder packet.", ""});
    }

    for (;;) {
        const int result = avcodec_receive_packet(encoder, packet.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return Result<void>::Ok();
        }
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "encoder_receive_packet_failed",
                "NVENC did not return an encoded packet.",
                result));
        }

        packet->stream_index = stream->index;
        av_packet_rescale_ts(packet.get(), encoder->time_base, stream->time_base);

        const int write_result = av_interleaved_write_frame(output, packet.get());
        av_packet_unref(packet.get());
        if (write_result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "output_packet_write_failed",
                "FFmpeg could not mux an encoded packet.",
                write_result));
        }
    }
}

class RtxShutdownGuard {
public:
    explicit RtxShutdownGuard(RtxProcessor* rtx) : rtx_(rtx) {}
    ~RtxShutdownGuard() {
        if (active_ && rtx_ != nullptr) {
            rtx_->shutdown();
        }
    }

    void activate() { active_ = true; }
    void release() {
        if (active_ && rtx_ != nullptr) {
            rtx_->shutdown();
        }
        active_ = false;
    }

private:
    RtxProcessor* rtx_ = nullptr;
    bool active_ = false;
};

#endif

} // namespace

FfmpegTranscodePipeline::FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx) : rtx_(std::move(rtx)) {}

Result<void> FfmpegTranscodePipeline::run(
    const TranscodeRequest& request,
    CancellationToken& cancellation,
    ProgressCallback progress) {
    log_info("FFmpeg pipeline starting: " + request.input_path + " -> " + request.output_path);
    std::error_code input_status_error;
    const bool input_exists = std::filesystem::exists(request.input_path, input_status_error);
    if (input_status_error) {
        return Result<void>::Fail({
            "input_access_failed",
            "Input file could not be checked.",
            request.input_path + ": " + input_status_error.message()
        });
    }
    if (!input_exists) {
        return Result<void>::Fail({"input_not_found", "Input file does not exist.", request.input_path});
    }
    if (rtx_ == nullptr) {
        return Result<void>::Fail({
            "rtx_processor_required",
            "The FFmpeg hardware pipeline requires an RTX processor.",
            "Build with VSR_ENABLE_RTX_SDK=ON to enable RTX processing."
        });
    }
    auto canceled_result = [](const std::string& detail) -> Result<void> {
        log_info("FFmpeg pipeline canceled: " + detail + ".");
        return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
    };
    if (cancellation.requested.load()) {
        return canceled_result("before initialization");
    }

    if (progress) {
        JobProgress validating;
        validating.stage = JobStage::validating;
        validating.progress = 0.01;
        progress(validating);
    }

#if !defined(_WIN32)
    (void)progress_from_frames;
    return Result<void>::Fail({
        "d3d11_required",
        "The FFmpeg hardware pipeline requires Windows D3D11.",
        "Run the hardware backend on Windows with D3D11VA and NVENC support."
    });
#else
    AVFormatContext* raw_input = nullptr;
    int result = avformat_open_input(&raw_input, request.input_path.c_str(), nullptr, nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "input_open_failed",
            "FFmpeg could not open the input file.",
            result));
    }
    InputFormatContextPtr input(raw_input);

    result = avformat_find_stream_info(input.get(), nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "stream_info_failed",
            "FFmpeg could not read stream information.",
            result));
    }

    const int video_stream_index = find_primary_video_stream(input.get());
    if (video_stream_index < 0) {
        return Result<void>::Fail({"video_stream_missing", "Input file does not contain a video stream.", request.input_path});
    }
    AVStream* input_stream = input->streams[video_stream_index];
    const AVCodec* decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    if (decoder == nullptr) {
        return Result<void>::Fail({"decoder_missing", "FFmpeg does not have a decoder for the primary video stream.", ""});
    }

    AVRational frame_rate = av_guess_frame_rate(input.get(), input_stream, nullptr);
    if (!valid_rational(frame_rate)) {
        frame_rate = input_stream->avg_frame_rate;
    }
    if (!valid_rational(frame_rate)) {
        frame_rate = {30, 1};
    }
    const std::int64_t frames_total = estimate_total_frames(input.get(), input_stream, frame_rate);
    if (progress) {
        JobProgress probing;
        probing.stage = JobStage::probing;
        probing.progress = 0.05;
        probing.frames_total = frames_total;
        progress(probing);
    }

    const auto d3d11 = create_d3d11_device();
    if (!d3d11.ok()) {
        return Result<void>::Fail(d3d11.error());
    }
    if (progress) {
        JobProgress initializing;
        initializing.stage = JobStage::initializing_gpu;
        initializing.progress = 0.08;
        initializing.frames_total = frames_total;
        progress(initializing);
    }

    auto hw_device = create_ffmpeg_d3d11_device(d3d11.value().device.Get(), d3d11.value().context.Get());
    if (!hw_device.ok()) {
        return Result<void>::Fail(hw_device.error());
    }

    CodecContextPtr decoder_context(avcodec_alloc_context3(decoder));
    if (!decoder_context) {
        return Result<void>::Fail({"decoder_context_alloc_failed", "FFmpeg could not allocate the decoder context.", ""});
    }
    result = avcodec_parameters_to_context(decoder_context.get(), input_stream->codecpar);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "decoder_parameters_failed",
            "FFmpeg could not copy decoder parameters.",
            result));
    }
    decoder_context->pkt_timebase = input_stream->time_base;
    decoder_context->get_format = choose_d3d11_format;
    decoder_context->hw_device_ctx = av_buffer_ref(hw_device.value().get());
    if (decoder_context->hw_device_ctx == nullptr) {
        return Result<void>::Fail({"d3d11va_device_ref_failed", "FFmpeg could not reference the D3D11VA device context for decoding.", ""});
    }

    result = avcodec_open2(decoder_context.get(), decoder, nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "decoder_open_failed",
            "FFmpeg could not open the D3D11VA decoder.",
            result));
    }

    AVFormatContext* raw_output = nullptr;
    result = avformat_alloc_output_context2(&raw_output, nullptr, "mp4", request.output_path.c_str());
    if (result < 0 || raw_output == nullptr) {
        return Result<void>::Fail(result < 0
            ? ffmpeg_error("output_context_alloc_failed", "FFmpeg could not create an MP4 output context.", result)
            : Error{"output_context_alloc_failed", "FFmpeg could not create an MP4 output context.", request.output_path});
    }
    OutputFormatContextPtr output(raw_output);

    const bool hdr_enabled = request.processing.hdr.enabled;
    const char* encoder_name = ffmpeg_nvenc_encoder_name(request.output, hdr_enabled);
    const AVCodec* encoder = avcodec_find_encoder_by_name(encoder_name);
    if (encoder == nullptr) {
        return Result<void>::Fail({
            "nvenc_encoder_missing",
            "FFmpeg does not expose the required NVENC encoder.",
            encoder_name
        });
    }

    const double scale = request.processing.vsr.enabled ? request.processing.vsr.scale : 1.0;
    const int output_width = even_scaled_dimension(decoder_context->width, scale);
    const int output_height = even_scaled_dimension(decoder_context->height, scale);
    const AVPixelFormat encoder_sw_format = hdr_enabled ? AV_PIX_FMT_X2BGR10 : AV_PIX_FMT_BGRA;
    const DXGI_FORMAT rtx_dxgi_format = dxgi_format_for_encoder_surface(encoder_sw_format);
    if (rtx_dxgi_format == DXGI_FORMAT_UNKNOWN) {
        return Result<void>::Fail({"encoder_format_unsupported", "The requested encoder texture format is not supported.", ""});
    }

    auto encoder_frames = create_encoder_frames_context(
        hw_device.value().get(),
        encoder_sw_format,
        output_width,
        output_height);
    if (!encoder_frames.ok()) {
        return Result<void>::Fail(encoder_frames.error());
    }

    CodecContextPtr encoder_context(avcodec_alloc_context3(encoder));
    if (!encoder_context) {
        return Result<void>::Fail({"encoder_context_alloc_failed", "FFmpeg could not allocate the NVENC context.", ""});
    }
    encoder_context->width = output_width;
    encoder_context->height = output_height;
    encoder_context->time_base = av_inv_q(frame_rate);
    encoder_context->framerate = frame_rate;
    encoder_context->pix_fmt = AV_PIX_FMT_D3D11;
    encoder_context->sw_pix_fmt = encoder_sw_format;
    encoder_context->sample_aspect_ratio = decoder_context->sample_aspect_ratio;
    const std::int64_t source_bit_rate = source_video_bit_rate(input.get(), input_stream, decoder_context.get());
    const std::int64_t target_bit_rate = ffmpeg_recommended_nvenc_bitrate(
        source_bit_rate,
        decoder_context->width,
        decoder_context->height,
        output_width,
        output_height,
        av_q2d(frame_rate),
        hdr_enabled);
    encoder_context->bit_rate = target_bit_rate;
    encoder_context->rc_max_rate = target_bit_rate + (target_bit_rate / 2);
    encoder_context->rc_buffer_size = target_bit_rate * 2;
    encoder_context->hw_frames_ctx = av_buffer_ref(encoder_frames.value().get());
    if (encoder_context->hw_frames_ctx == nullptr) {
        return Result<void>::Fail({"encoder_frames_ref_failed", "FFmpeg could not reference the encoder D3D11 frames.", ""});
    }
    if (output->oformat != nullptr && (output->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (hdr_enabled) {
        encoder_context->profile = 2;
        encoder_context->color_primaries = AVCOL_PRI_BT2020;
        encoder_context->color_trc = AVCOL_TRC_SMPTE2084;
        encoder_context->colorspace = AVCOL_SPC_BT2020_NCL;
        encoder_context->color_range = AVCOL_RANGE_MPEG;
        av_opt_set(encoder_context->priv_data, "profile", "main10", 0);
        av_opt_set(encoder_context->priv_data, "tier", "main", 0);
    } else {
        encoder_context->color_primaries = decoder_context->color_primaries;
        encoder_context->color_trc = decoder_context->color_trc;
        encoder_context->colorspace = decoder_context->colorspace;
        encoder_context->color_range = decoder_context->color_range;
    }
    const auto quality_configured = configure_nvenc_quality(encoder_context.get());
    if (!quality_configured.ok()) {
        return Result<void>::Fail(quality_configured.error());
    }
    log_info(
        "NVENC quality configured: target_bitrate=" + std::to_string(target_bit_rate) +
        ", maxrate=" + std::to_string(encoder_context->rc_max_rate) +
        ", buffer=" + std::to_string(encoder_context->rc_buffer_size));

    result = avcodec_open2(encoder_context.get(), encoder, nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "encoder_open_failed",
            "FFmpeg could not open the NVENC encoder.",
            result));
    }

    AVStream* output_stream = avformat_new_stream(output.get(), nullptr);
    if (output_stream == nullptr) {
        return Result<void>::Fail({"output_stream_create_failed", "FFmpeg could not create the output video stream.", ""});
    }
    output_stream->time_base = encoder_context->time_base;
    result = avcodec_parameters_from_context(output_stream->codecpar, encoder_context.get());
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "encoder_parameters_failed",
            "FFmpeg could not copy encoder parameters to the output stream.",
            result));
    }

    struct CopiedStream {
        AVStream* output_stream = nullptr;
    };
    std::vector<CopiedStream> copied_streams(input->nb_streams);
    if (ffmpeg_requests_stream_copy(request.output)) {
        for (unsigned int index = 0; index < input->nb_streams; ++index) {
            if (static_cast<int>(index) == video_stream_index) {
                continue;
            }

            AVStream* source_stream = input->streams[index];
            if (source_stream == nullptr || source_stream->codecpar == nullptr) {
                continue;
            }

            const AVMediaType media_type = source_stream->codecpar->codec_type;
            if (media_type == AVMEDIA_TYPE_AUDIO) {
                if (request.output.audio_mode != "copy") {
                    continue;
                }
                if (!mp4_audio_copy_compatible(source_stream->codecpar->codec_id)) {
                    return Result<void>::Fail({
                        "unsupported_mp4_stream",
                        "Audio stream cannot be copied into MP4 output.",
                        codec_details(source_stream)
                    });
                }
            } else if (media_type == AVMEDIA_TYPE_SUBTITLE) {
                if (request.output.subtitle_mode != "copy-compatible") {
                    continue;
                }
                if (!mp4_subtitle_copy_compatible(source_stream->codecpar->codec_id)) {
                    return Result<void>::Fail({
                        "unsupported_mp4_stream",
                        "Subtitle stream cannot be copied into MP4 output.",
                        codec_details(source_stream)
                    });
                }
            } else {
                continue;
            }

            AVStream* copied_stream = avformat_new_stream(output.get(), nullptr);
            if (copied_stream == nullptr) {
                return Result<void>::Fail({"output_stream_create_failed", "FFmpeg could not create a copied output stream.", codec_details(source_stream)});
            }
            result = avcodec_parameters_copy(copied_stream->codecpar, source_stream->codecpar);
            if (result < 0) {
                return Result<void>::Fail(ffmpeg_error(
                    "copy_stream_parameters_failed",
                    "FFmpeg could not copy input stream parameters to the MP4 output.",
                    result));
            }
            copied_stream->codecpar->codec_tag = 0;
            copied_stream->time_base = source_stream->time_base;
            copied_streams[index].output_stream = copied_stream;
        }
    }

    const auto initialized = rtx_->initialize(d3d11.value().device.Get(), request.processing);
    if (!initialized.ok()) {
        return Result<void>::Fail(initialized.error());
    }
    RtxShutdownGuard rtx_shutdown(rtx_.get());
    rtx_shutdown.activate();

    if (output->oformat != nullptr && (output->oformat->flags & AVFMT_NOFILE) == 0) {
        result = avio_open(&output->pb, request.output_path.c_str(), AVIO_FLAG_WRITE);
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "output_open_failed",
                "FFmpeg could not open the output file for writing.",
                result));
        }
    }

    result = avformat_write_header(output.get(), nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "output_header_failed",
            "FFmpeg could not write the MP4 header.",
            result));
    }

    PacketPtr packet(av_packet_alloc());
    FramePtr decoded_frame(av_frame_alloc());
    if (!packet || !decoded_frame) {
        return Result<void>::Fail({"ffmpeg_frame_alloc_failed", "FFmpeg could not allocate decode buffers.", ""});
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> rtx_input_texture;
    std::int64_t frames_done = 0;
    const DXGI_COLOR_SPACE_TYPE video_processor_input_color = d3d11_input_color_space(decoder_context.get());
    const DXGI_COLOR_SPACE_TYPE video_processor_output_color = d3d11_rtx_input_color_space(decoder_context.get());

    auto emit_progress = [&](JobStage stage, std::int64_t done) {
        if (!progress) {
            return;
        }
        JobProgress job_progress;
        job_progress.stage = stage;
        job_progress.progress = progress_from_frames(done, frames_total);
        job_progress.frames_done = done;
        job_progress.frames_total = frames_total;
        progress(job_progress);
    };

    auto process_decoded_frame = [&](AVFrame* frame) -> Result<void> {
        if (cancellation.requested.load()) {
            return canceled_result("before processing decoded frame");
        }
        if (frame->format != AV_PIX_FMT_D3D11 || frame->data[0] == nullptr) {
            return Result<void>::Fail({
                "d3d11_frame_required",
                "The hardware pipeline requires FFmpeg D3D11VA frames.",
                "Use a Windows FFmpeg build with D3D11VA and NVENC support."
            });
        }

        auto* decoded_texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
        const UINT decoded_slice = static_cast<UINT>(reinterpret_cast<intptr_t>(frame->data[1]));

        if (rtx_input_texture == nullptr) {
            const auto created = create_texture(
                d3d11.value().device.Get(),
                decoder_context->width,
                decoder_context->height,
                rtx_dxgi_format,
                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
                rtx_input_texture);
            if (!created.ok()) {
                return Result<void>::Fail(created.error());
            }
        }

        const auto converted = convert_texture_with_video_processor(
            d3d11.value().device.Get(),
            d3d11.value().context.Get(),
            decoded_texture,
            decoded_slice,
            rtx_input_texture.Get(),
            0,
            decoder_context->width,
            decoder_context->height,
            decoder_context->width,
            decoder_context->height,
            video_processor_input_color,
            video_processor_output_color);
        if (!converted.ok()) {
            return Result<void>::Fail(converted.error());
        }

        FramePtr encoder_frame(av_frame_alloc());
        if (!encoder_frame) {
            return Result<void>::Fail({"encoder_frame_alloc_failed", "FFmpeg could not allocate an encoder frame.", ""});
        }
        result = av_hwframe_get_buffer(encoder_frames.value().get(), encoder_frame.get(), 0);
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "encoder_frame_get_buffer_failed",
                "FFmpeg could not allocate a D3D11 encoder frame.",
                result));
        }

        auto* output_texture = reinterpret_cast<ID3D11Texture2D*>(encoder_frame->data[0]);
        const UINT output_slice = static_cast<UINT>(reinterpret_cast<intptr_t>(encoder_frame->data[1]));
        if (output_texture == nullptr) {
            return Result<void>::Fail({"encoder_frame_texture_missing", "The D3D11 encoder frame did not expose a texture.", ""});
        }
        if (output_slice != 0) {
            return Result<void>::Fail({
                "encoder_array_slice_unsupported",
                "The RTX processor requires a standalone D3D11 output texture.",
                "FFmpeg allocated an array-slice encoder frame."
            });
        }

        D3D11_TEXTURE2D_DESC output_desc = {};
        output_texture->GetDesc(&output_desc);
        if (output_desc.Format != rtx_dxgi_format) {
            return Result<void>::Fail({
                "encoder_texture_format_mismatch",
                "The NVENC D3D11 frame format does not match the RTX output format.",
                ""
            });
        }

        emit_progress(JobStage::processing_rtx, frames_done);
        RtxDx11Frame rtx_frame;
        rtx_frame.input = rtx_input_texture.Get();
        rtx_frame.output = output_texture;
        rtx_frame.input_width = static_cast<std::uint32_t>(decoder_context->width);
        rtx_frame.input_height = static_cast<std::uint32_t>(decoder_context->height);
        rtx_frame.output_width = static_cast<std::uint32_t>(output_width);
        rtx_frame.output_height = static_cast<std::uint32_t>(output_height);

        const auto processed = rtx_->process(rtx_frame, request.processing);
        if (!processed.ok()) {
            return Result<void>::Fail(processed.error());
        }

        encoder_frame->pts = frame->pts == AV_NOPTS_VALUE
            ? frames_done
            : av_rescale_q(frame->pts, input_stream->time_base, encoder_context->time_base);
        encoder_frame->duration = frame->duration > 0
            ? av_rescale_q(frame->duration, input_stream->time_base, encoder_context->time_base)
            : 0;
        encoder_frame->width = output_width;
        encoder_frame->height = output_height;
        encoder_frame->format = AV_PIX_FMT_D3D11;

        result = avcodec_send_frame(encoder_context.get(), encoder_frame.get());
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "encoder_send_frame_failed",
                "NVENC could not accept a processed frame.",
                result));
        }
        ++frames_done;
        emit_progress(JobStage::encoding, frames_done);

        return drain_encoder(encoder_context.get(), output.get(), output_stream);
    };

    auto receive_decoder_frames = [&]() -> Result<void> {
        for (;;) {
            av_frame_unref(decoded_frame.get());
            result = avcodec_receive_frame(decoder_context.get(), decoded_frame.get());
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                return Result<void>::Ok();
            }
            if (result < 0) {
                return Result<void>::Fail(ffmpeg_error(
                    "decoder_receive_frame_failed",
                    "FFmpeg could not receive a decoded frame.",
                    result));
            }

            const auto processed = process_decoded_frame(decoded_frame.get());
            if (!processed.ok()) {
                return Result<void>::Fail(processed.error());
            }
        }
    };

    for (;;) {
        if (cancellation.requested.load()) {
            return canceled_result("while reading input");
        }
        av_packet_unref(packet.get());
        result = av_read_frame(input.get(), packet.get());
        if (result == AVERROR_EOF) {
            break;
        }
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "input_read_failed",
                "FFmpeg could not read the next input packet.",
                result));
        }
        if (packet->stream_index != video_stream_index) {
            if (packet->stream_index >= 0 && static_cast<std::size_t>(packet->stream_index) < copied_streams.size()) {
                AVStream* copied_stream = copied_streams[packet->stream_index].output_stream;
                if (copied_stream != nullptr) {
                    AVStream* source_stream = input->streams[packet->stream_index];
                    packet->stream_index = copied_stream->index;
                    av_packet_rescale_ts(packet.get(), source_stream->time_base, copied_stream->time_base);
                    const int write_result = av_interleaved_write_frame(output.get(), packet.get());
                    if (write_result < 0) {
                        return Result<void>::Fail(ffmpeg_error(
                            "output_copy_packet_write_failed",
                            "FFmpeg could not mux a copied stream packet.",
                            write_result));
                    }
                }
            }
            continue;
        }

        emit_progress(JobStage::decoding, frames_done);
        result = avcodec_send_packet(decoder_context.get(), packet.get());
        if (result < 0) {
            return Result<void>::Fail(ffmpeg_error(
                "decoder_send_packet_failed",
                "FFmpeg could not send a packet to the decoder.",
                result));
        }

        const auto received = receive_decoder_frames();
        if (!received.ok()) {
            return Result<void>::Fail(received.error());
        }
    }

    if (cancellation.requested.load()) {
        return canceled_result("before decoder flush");
    }
    result = avcodec_send_packet(decoder_context.get(), nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "decoder_flush_failed",
            "FFmpeg could not flush the decoder.",
            result));
    }
    const auto decoded_flush = receive_decoder_frames();
    if (!decoded_flush.ok()) {
        return Result<void>::Fail(decoded_flush.error());
    }

    if (cancellation.requested.load()) {
        return canceled_result("before encoder flush");
    }
    result = avcodec_send_frame(encoder_context.get(), nullptr);
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "encoder_flush_failed",
            "NVENC could not flush.",
            result));
    }
    const auto encoded_flush = drain_encoder(encoder_context.get(), output.get(), output_stream);
    if (!encoded_flush.ok()) {
        return Result<void>::Fail(encoded_flush.error());
    }

    if (progress) {
        JobProgress muxing;
        muxing.stage = JobStage::muxing;
        muxing.progress = 0.99;
        muxing.frames_done = frames_done;
        muxing.frames_total = frames_total;
        progress(muxing);
    }

    if (cancellation.requested.load()) {
        return canceled_result("before final muxing");
    }
    result = av_write_trailer(output.get());
    if (result < 0) {
        return Result<void>::Fail(ffmpeg_error(
            "output_trailer_failed",
            "FFmpeg could not write the MP4 trailer.",
            result));
    }

    rtx_shutdown.release();
    log_info("FFmpeg pipeline completed: " + request.output_path);
    if (progress) {
        JobProgress finalizing;
        finalizing.stage = JobStage::finalizing;
        finalizing.progress = 1.0;
        finalizing.frames_done = frames_done;
        finalizing.frames_total = frames_total;
        progress(finalizing);
    }
    return Result<void>::Ok();
#endif
}

} // namespace vsr
