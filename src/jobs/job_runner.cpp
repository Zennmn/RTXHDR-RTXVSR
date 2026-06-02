#include "jobs/job_runner.h"

#include "platform/logging.h"

namespace vsr {
namespace {

std::string describe_error(const Error& error) {
    std::string description = error.code + ": " + error.message;
    if (!error.details.empty()) {
        description += " (" + error.details + ")";
    }
    return description;
}

} // namespace

JobRunner::JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline)
    : store_(store), pipeline_(std::move(pipeline)) {}

Result<void> JobRunner::run_one(const std::string& id) {
    std::lock_guard lock(run_mutex_);

    const auto request = store_.start(id);
    if (!request.ok()) {
        if (request.error().code == "job_canceled") {
            log_info("Job " + id + " canceled before pipeline start.");
        } else {
            log_error("Job " + id + " failed to start: " + describe_error(request.error()));
        }
        return Result<void>::Fail(request.error());
    }
    log_info("Job " + id + " started.");

    if (pipeline_ == nullptr) {
        Error error{"pipeline_unavailable", "Video pipeline is not available.", ""};
        store_.mark_failed(id, error);
        log_error("Job " + id + " failed: " + describe_error(error));
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
        if (marked.ok()) {
            log_info("Job " + id + " succeeded.");
        } else if (marked.error().code == "job_canceled") {
            log_info("Job " + id + " canceled before success could be recorded.");
        } else {
            log_error("Job " + id + " could not record success: " + describe_error(marked.error()));
        }
        return marked;
    }

    const auto marked = store_.mark_failed(id, result.error());
    {
        std::lock_guard cancellation_lock(cancellation_mutex_);
        active_cancellations_.erase(id);
    }
    if (result.error().code == "job_canceled") {
        log_info("Job " + id + " canceled.");
    } else {
        log_error("Job " + id + " failed: " + describe_error(result.error()));
    }
    if (!marked.ok()) {
        log_error("Job " + id + " could not record failure: " + describe_error(marked.error()));
        return marked;
    }
    return Result<void>::Fail(result.error());
}

Result<void> JobRunner::request_cancel(const std::string& id) {
    const auto canceled = store_.request_cancel(id);
    if (!canceled.ok()) {
        log_error("Cancel request for job " + id + " rejected: " + describe_error(canceled.error()));
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
        log_info("Cancel requested for job " + id + "; active pipeline token signaled.");
    } else {
        log_info("Cancel requested for job " + id + ".");
    }

    return Result<void>::Ok();
}

} // namespace vsr
