#pragma once

#include "jobs/job_store.h"
#include "video/video_pipeline.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vsr {

class JobRunner {
public:
    JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline);
    Result<void> run_one(const std::string& id);
    Result<void> request_cancel(const std::string& id);

private:
    std::mutex run_mutex_;
    std::mutex cancellation_mutex_;
    std::unordered_map<std::string, std::shared_ptr<CancellationToken>> active_cancellations_;
    JobStore& store_;
    std::unique_ptr<VideoPipeline> pipeline_;
};

} // namespace vsr
