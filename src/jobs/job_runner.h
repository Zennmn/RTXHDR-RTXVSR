#pragma once

#include "jobs/job_store.h"
#include "video/video_pipeline.h"

#include <memory>
#include <mutex>

namespace vsr {

class JobRunner {
public:
    JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline);
    Result<void> run_one(const std::string& id);

private:
    std::mutex run_mutex_;
    JobStore& store_;
    std::unique_ptr<VideoPipeline> pipeline_;
};

} // namespace vsr
