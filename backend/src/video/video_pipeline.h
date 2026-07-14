#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <functional>
#include <string>

namespace vsr {

using ProgressCallback = std::function<void(const JobProgress&)>;
using WarningCallback = std::function<void(const std::string&)>;

class VideoPipeline {
public:
    virtual ~VideoPipeline() = default;
    virtual Result<void> run(
        const TranscodeRequest& request,
        CancellationToken& cancellation,
        ProgressCallback progress,
        WarningCallback warning) = 0;
};

} // namespace vsr
