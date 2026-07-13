#include "video/media_probe_service.h"

#if defined(VSR_ENABLE_FFMPEG)
#include "video/ffmpeg/ffmpeg_probe.h"
#endif

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace vsr {

std::string format_duration_seconds(double duration_seconds) {
    if (duration_seconds <= 0.0) {
        return "";
    }

    const auto rounded = static_cast<int>(std::llround(duration_seconds));
    const int hours = rounded / 3600;
    const int minutes = (rounded % 3600) / 60;
    const int seconds = rounded % 60;

    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << hours << ":"
           << std::setw(2) << minutes << ":"
           << std::setw(2) << seconds;
    return stream.str();
}

Result<MediaProbeSummary> probe_media_for_ui(const std::string& path) {
    if (path.empty()) {
        return Result<MediaProbeSummary>::Fail({"input_path_required", "Input path is required.", ""});
    }

    std::filesystem::path input(path);
    std::error_code error;
    if (!std::filesystem::exists(input, error)) {
        return Result<MediaProbeSummary>::Fail({"input_not_found", "Input file was not found.", path});
    }
    if (!std::filesystem::is_regular_file(input, error)) {
        return Result<MediaProbeSummary>::Fail({"input_not_file", "Input path is not a file.", path});
    }

    MediaProbeSummary summary;
    summary.path = path;
    summary.name = input.filename().string();

    error.clear();
    const auto size = std::filesystem::file_size(input, error);
    if (error) {
        summary.warnings.push_back("File size could not be read.");
    } else {
        summary.size_bytes = static_cast<std::uint64_t>(size);
    }

#if defined(VSR_ENABLE_FFMPEG)
    const auto detailed = probe_media(path);
    if (!detailed.ok()) {
        summary.warnings.push_back(
            "FFmpeg could not read detailed media metadata: " + detailed.error().message);
    } else {
        const auto& media = detailed.value();
        if (media.width > 0 && media.height > 0) {
            summary.resolution = std::to_string(media.width) + "x" + std::to_string(media.height);
        }
        summary.duration = format_duration_seconds(media.duration_seconds);
        summary.codec = media.codec_name;
        if (media.bit_depth > 0) {
            summary.codec += " " + std::to_string(media.bit_depth) + "-bit";
        }
        summary.warnings.insert(summary.warnings.end(), media.warnings.begin(), media.warnings.end());
    }
#else
    summary.warnings.push_back("FFmpeg support is not enabled; detailed media metadata is unavailable.");
#endif

    return Result<MediaProbeSummary>::Ok(std::move(summary));
}

} // namespace vsr
