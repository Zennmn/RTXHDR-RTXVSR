#include "rtx/video_io/ffmpeg_probe.h"

#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_desc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
}

namespace rtx {

namespace {

std::string SafeString(const char* value) { return value != nullptr ? value : "unknown"; }

double RationalToDouble(AVRational value) {
  if (value.den == 0) {
    return 0.0;
  }
  return av_q2d(value);
}

int ResolveBitDepth(const AVCodecParameters* codecpar) {
  if (codecpar->bits_per_raw_sample > 0) {
    return codecpar->bits_per_raw_sample;
  }

  const AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(codecpar->format);
  const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(pix_fmt);
  if (descriptor != nullptr && descriptor->nb_components > 0) {
    return descriptor->comp[0].depth;
  }

  return 8;
}

std::string ColorPrimariesToString(AVColorPrimaries value) {
  const char* text = av_color_primaries_name(value);
  return text != nullptr ? text : "unknown";
}

std::string TransferToString(AVColorTransferCharacteristic value) {
  const char* text = av_color_transfer_name(value);
  return text != nullptr ? text : "unknown";
}

std::string MatrixToString(AVColorSpace value) {
  const char* text = av_color_space_name(value);
  return text != nullptr ? text : "unknown";
}

std::string RangeToString(AVColorRange value) {
  switch (value) {
    case AVCOL_RANGE_JPEG:
      return "full";
    case AVCOL_RANGE_MPEG:
      return "limited";
    default:
      return "unknown";
  }
}

bool IsHdrTransfer(AVColorTransferCharacteristic value) {
  return value == AVCOL_TRC_SMPTE2084 || value == AVCOL_TRC_ARIB_STD_B67;
}

const AVCodec* FindByName(const char* name, bool encoder) {
  return encoder ? avcodec_find_encoder_by_name(name) : avcodec_find_decoder_by_name(name);
}

}  // namespace

FfmpegProbe::FfmpegProbe(Logger& logger) : logger_(logger) {
  av_log_set_level(AV_LOG_ERROR);
}

Status FfmpegProbe::Probe(const std::string& path, MediaInfo& info) const {
  if (!std::filesystem::exists(path)) {
    return Status::NotFound("输入文件不存在: " + path);
  }

  AVFormatContext* format_context = nullptr;
  if (avformat_open_input(&format_context, path.c_str(), nullptr, nullptr) < 0) {
    return Status::IoError("FFmpeg 无法打开输入文件。");
  }

  auto cleanup = [&]() {
    if (format_context != nullptr) {
      avformat_close_input(&format_context);
    }
  };

  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    cleanup();
    return Status::IoError("FFmpeg 无法读取输入流信息。");
  }

  info = MediaInfo{};
  info.path = path;
  info.container_name = SafeString(format_context->iformat != nullptr ? format_context->iformat->name : nullptr);
  if (format_context->duration > 0) {
    info.duration_seconds = static_cast<double>(format_context->duration) / AV_TIME_BASE;
  }

  for (unsigned int index = 0; index < format_context->nb_streams; ++index) {
    const AVStream* stream = format_context->streams[index];
    const AVCodecParameters* codecpar = stream->codecpar;
    const AVCodecDescriptor* descriptor = avcodec_descriptor_get(codecpar->codec_id);
    const std::string codec_name = descriptor != nullptr ? descriptor->name : "unknown";

    if (!info.has_video && codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      info.has_video = true;
      auto& video = info.primary_video;
      video.index = static_cast<int>(index);
      video.codec_name = codec_name;
      video.width = codecpar->width;
      video.height = codecpar->height;
      video.frame_rate = RationalToDouble(stream->avg_frame_rate);
      video.frame_rate_num = stream->avg_frame_rate.num;
      video.frame_rate_den = stream->avg_frame_rate.den;
      video.frame_count = stream->nb_frames;
      video.color.pixel_format =
          SafeString(av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecpar->format)));
      video.color.primaries =
          ColorPrimariesToString(static_cast<AVColorPrimaries>(codecpar->color_primaries));
      video.color.transfer = TransferToString(
          static_cast<AVColorTransferCharacteristic>(codecpar->color_trc));
      video.color.matrix = MatrixToString(static_cast<AVColorSpace>(codecpar->color_space));
      video.color.range = RangeToString(static_cast<AVColorRange>(codecpar->color_range));
      video.color.bit_depth = ResolveBitDepth(codecpar);
      video.color.hdr_signaled =
          IsHdrTransfer(static_cast<AVColorTransferCharacteristic>(codecpar->color_trc));
      continue;
    }

    if (!info.has_audio && codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      info.has_audio = true;
      auto& audio = info.primary_audio;
      audio.index = static_cast<int>(index);
      audio.codec_name = codec_name;
#if LIBAVUTIL_VERSION_MAJOR >= 57
      audio.channels = codecpar->ch_layout.nb_channels;
#else
      audio.channels = codecpar->channels;
#endif
      audio.sample_rate = codecpar->sample_rate;
    }
  }

  cleanup();

  if (!info.has_video) {
    return Status::Unsupported("输入文件不包含可处理的视频流。");
  }

  logger_.Debug("Probed input with FFmpeg: " + info.container_name);
  return Status::Ok();
}

bool FfmpegProbe::CanDecodeWithHardware(const std::string& codec_name) const {
  if (codec_name == "h264") {
    return FindByName("h264_cuvid", false) != nullptr;
  }
  if (codec_name == "hevc") {
    return FindByName("hevc_cuvid", false) != nullptr;
  }
  if (codec_name == "av1") {
    return FindByName("av1_cuvid", false) != nullptr;
  }
  return false;
}

bool FfmpegProbe::CanEncodeWithHardware(const std::string& codec_name) const {
  if (codec_name == "h264") {
    return FindByName("h264_nvenc", true) != nullptr;
  }
  if (codec_name == "hevc") {
    return FindByName("hevc_nvenc", true) != nullptr;
  }
  if (codec_name == "av1") {
    return FindByName("av1_nvenc", true) != nullptr;
  }
  return false;
}

}  // namespace rtx

