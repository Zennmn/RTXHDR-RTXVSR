#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

class JobManager {
 public:
  struct Impl;

  JobManager();
  ~JobManager();

  Status CreateJob(const JobConfig& config, std::uint64_t& job_id);
  Status StartJob(std::uint64_t job_id, const RuntimePaths& runtime_paths, const SystemProbe& system_probe);
  Status QueryJobProgress(std::uint64_t job_id, JobProgress& progress) const;
  Status CancelJob(std::uint64_t job_id);
  Status DestroyJob(std::uint64_t job_id);
  Status DrainEvents(std::uint64_t job_id, std::size_t max_events, std::vector<LogEvent>& events);

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtx
