#include "jobs/job_runner.h"

namespace vsr {

JobRunner::JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline)
    : store_(store), pipeline_(std::move(pipeline)) {}

Result<void> JobRunner::run_one(const std::string& id) {
    const auto request = store_.start(id);
    if (!request.ok()) {
        return Result<void>::Fail(request.error());
    }

    if (pipeline_ == nullptr) {
        Error error{"pipeline_unavailable", "Video pipeline is not available.", ""};
        store_.mark_failed(id, error);
        return Result<void>::Fail(error);
    }

    CancellationToken cancellation;

    const auto result = pipeline_->run(
        request.value(),
        cancellation,
        [this, &id](const JobProgress& progress) {
            store_.update_progress(id, progress);
        });

    if (result.ok()) {
        return store_.mark_succeeded(id);
    }

    const auto marked = store_.mark_failed(id, result.error());
    if (!marked.ok()) {
        return marked;
    }
    return Result<void>::Fail(result.error());
}

} // namespace vsr
