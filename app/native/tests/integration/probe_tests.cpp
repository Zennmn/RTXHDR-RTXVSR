#include <iostream>

#include "rtx/core/logger.h"
#include "rtx/video_io/ffmpeg_probe.h"

int main() {
  rtx::Logger logger(rtx::LogLevel::kError);
  rtx::FfmpegProbe probe(logger);

  if (!probe.CanDecodeWithHardware("h264")) {
    std::cerr << "warning: h264 cuvid decoder was not detected\n";
  }

  return 0;
}

