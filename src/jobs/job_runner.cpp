#include "jobs/job_runner.h"

namespace vsr {

JobRunner::JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline)
    : store_(store), pipeline_(std::move(pipeline)) {}

Result<void> JobRunner::run_one(const std::string& id) {
    std::lock_guard lock(run_mutex_);

    const auto request = store_.start(id);
    if (!request.ok()) {
        return Result<void>::Fail(request.error());
    }

    if (pipeline_ == nullptr) {
        Error error{"pipeline_unavailable", "Video pipeline is not available.", ""};
        store_.mark_failed(id, error);
        return Result<void>::Fail(error);
    }

    auto cancellation = std::make_shared<CancellationToken>();
    {
        std::lock_guard cancellation_lock(cancellation_mutex_);
        active_cancellations_[id] = cancellation;
    }
    if (store_.is_cancel_requested(id)) {
        cancellation->requested.store(true);
    }

    const auto result = pipeline_->run(
        request.value(),
        *cancellation,
        [this, &id](const JobProgress& progress) {
            store_.update_progress(id, progress);
        });

    if (result.ok()) {
        const auto marked = store_.mark_succeeded(id);
        {
            std::lock_guard cancellation_lock(cancellation_mutex_);
            active_cancellations_.erase(id);
        }
        return marked;
    }

    const auto marked = store_.mark_failed(id, result.error());
    {
        std::lock_guard cancellation_lock(cancellation_mutex_);
        active_cancellations_.erase(id);
    }
    if (!marked.ok()) {
        return marked;
    }
    return Result<void>::Fail(result.error());
}

Result<void> JobRunner::request_cancel(const std::string& id) {
    const auto canceled = store_.request_cancel(id);
    if (!canceled.ok()) {
        return canceled;
    }

    std::shared_ptr<CancellationToken> cancellation;
    {
        std::lock_guard lock(cancellation_mutex_);
        const auto it = active_cancellations_.find(id);
        if (it != active_cancellations_.end()) {
            cancellation = it->second;
        }
    }

    if (cancellation != nullptr) {
        cancellation->requested.store(true);
    }

    return Result<void>::Ok();
}

} // namespace vsr
