#pragma once

#include "jobs/job_store.h"
#include "video/video_pipeline.h"

#include <memory>

namespace vsr {

class JobRunner {
public:
    JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline);
    void run_one(const std::string& id);

private:
    JobStore& store_;
    std::unique_ptr<VideoPipeline> pipeline_;
};

} // namespace vsr
