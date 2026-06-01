#include "jobs/job_runner.h"

namespace vsr {

JobRunner::JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline)
    : store_(store), pipeline_(std::move(pipeline)) {}

void JobRunner::run_one(const std::string& id) {
    const auto request = store_.get_request(id);
    if (!request.ok() || pipeline_ == nullptr) {
        return;
    }

    const bool cancel_requested = store_.is_cancel_requested(id);
    store_.mark_running(id);

    CancellationToken cancellation;
    cancellation.requested.store(cancel_requested);

    const auto result = pipeline_->run(
        request.value(),
        cancellation,
        [this, &id](const JobProgress& progress) {
            store_.update_progress(id, progress);
        });

    if (result.ok()) {
        store_.mark_succeeded(id);
    } else {
        store_.mark_failed(id, result.error());
    }
}

} // namespace vsr
