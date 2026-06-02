#include "video/ffmpeg/ffmpeg_probe.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>
#include <utility>

namespace vsr {
namespace {

struct FormatContextDeleter {
    void operator()(AVFormatContext* context) const noexcept {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

} // namespace

Result<MediaProbe> probe_media(const std::string& path) {
    AVFormatContext* raw_format = nullptr;
    if (avformat_open_input(&raw_format, path.c_str(), nullptr, nullptr) < 0) {
        return Result<MediaProbe>::Fail({"input_open_failed", "FFmpeg could not open the input file.", path});
    }
    FormatContextPtr format(raw_format);

    if (avformat_find_stream_info(format.get(), nullptr) < 0) {
        return Result<MediaProbe>::Fail({"stream_info_failed", "FFmpeg could not read stream information.", path});
    }

    MediaProbe probe;
    probe.path = path;
    if (format->duration != AV_NOPTS_VALUE && format->duration > 0) {
        probe.duration_seconds = static_cast<double>(format->duration) / static_cast<double>(AV_TIME_BASE);
    }

    const int video_stream_index = av_find_best_stream(format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        return Result<MediaProbe>::Fail({"video_stream_missing", "Input file does not contain a video stream.", path});
    }

    const AVStream* stream = format->streams[video_stream_index];
    if (stream == nullptr || stream->codecpar == nullptr || stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
        return Result<MediaProbe>::Fail({"video_stream_missing", "Input file does not contain a video stream.", path});
    }

    probe.video_stream_index = video_stream_index;
    probe.width = stream->codecpar->width;
    probe.height = stream->codecpar->height;
    probe.frame_count = stream->nb_frames;

    return Result<MediaProbe>::Ok(std::move(probe));
}

} // namespace vsr
