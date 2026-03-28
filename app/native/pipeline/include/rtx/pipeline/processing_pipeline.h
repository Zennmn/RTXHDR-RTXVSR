#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "rtx/core/logger.h"
#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

struct ProcessingCallbacks {
  std::function<bool()> is_cancelled;
  std::function<void(JobState state,
                     const std::string& phase,
                     double total_progress,
                     double phase_progress,
                     std::uint64_t processed_frames,
                     std::uint64_t total_frames,
                     double fps,
                     double elapsed_seconds,
                     double eta_seconds,
                     const std::string& message)>
      on_progress;
};

class ProcessingPipeline {
 public:
  ProcessingPipeline(const RuntimePaths& runtime_paths, Logger& logger);

  Status Run(const JobConfig& config,
             const SystemProbe& system_probe,
             const MediaInfo& input,
             const ProcessingCallbacks& callbacks);

 private:
  RuntimePaths runtime_paths_;
  Logger& logger_;
};

}  // namespace rtx

