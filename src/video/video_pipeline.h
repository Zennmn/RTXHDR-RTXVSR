#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <functional>

namespace vsr {

using ProgressCallback = std::function<void(const JobProgress&)>;

class VideoPipeline {
public:
    virtual ~VideoPipeline() = default;
    virtual Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) = 0;
};

} // namespace vsr
