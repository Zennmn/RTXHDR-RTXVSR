#include "jobs/job_store.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace vsr {

namespace {

std::string make_job_id(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(26) << value;
    return stream.str();
}

Error job_not_found(const std::string& id) {
    return {"job_not_found", "Job was not found.", id};
}

Error job_canceled() {
    return {"job_canceled", "Job was canceled.", ""};
}

Error job_already_finished(const std::string& id) {
    return {"job_already_finished", "Job is already finished.", id};
}

Error job_invalid_transition(const std::string& id) {
    return {"job_invalid_transition", "Job state transition is not allowed.", id};
}

bool is_terminal(JobState state) {
    return state == JobState::succeeded || state == JobState::failed || state == JobState::canceled;
}

void set_error(JobSnapshot& snapshot, const Error& error) {
    snapshot.error_code = error.code;
    snapshot.error_message = error.message;
    snapshot.error_details = error.details;
}

void touch(JobSnapshot& snapshot) {
    snapshot.updated_at = std::chrono::system_clock::now();
}

} // namespace

Result<std::string> JobStore::create(const TranscodeRequest& request) {
    const auto valid = validate_request(request);
    if (!valid.ok()) {
        return Result<std::string>::Fail(valid.error());
    }

    std::lock_guard lock(mutex_);
    const std::string id = make_job_id(next_id_++);
    const auto now = std::chrono::system_clock::now();

    JobRecord record;
    record.request = request;
    record.snapshot.id = id;
    record.snapshot.state = JobState::queued;
    record.snapshot.input_path = request.input_path;
    record.snapshot.output_path = request.output_path;
    record.snapshot.created_at = now;
    record.snapshot.updated_at = now;

    jobs_.emplace(id, std::move(record));
    return Result<std::string>::Ok(id);
}

Result<JobSnapshot> JobStore::get(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<JobSnapshot>::Fail(job_not_found(id));
    }
    return Result<JobSnapshot>::Ok(it->second.snapshot);
}

Result<TranscodeRequest> JobStore::get_request(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<TranscodeRequest>::Fail(job_not_found(id));
    }
    return Result<TranscodeRequest>::Ok(it->second.request);
}

Result<JobRecord> JobStore::get_record(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<JobRecord>::Fail(job_not_found(id));
    }
    return Result<JobRecord>::Ok(it->second);
}

Result<TranscodeRequest> JobStore::start(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<TranscodeRequest>::Fail(job_not_found(id));
    }

    auto& snapshot = it->second.snapshot;
    if (snapshot.state == JobState::queued) {
        snapshot.state = JobState::running;
        touch(snapshot);
        return Result<TranscodeRequest>::Ok(it->second.request);
    }
    if (snapshot.state == JobState::canceling) {
        const auto error = job_canceled();
        snapshot.state = JobState::canceled;
        set_error(snapshot, error);
        touch(snapshot);
        return Result<TranscodeRequest>::Fail(error);
    }
    if (is_terminal(snapshot.state)) {
        return Result<TranscodeRequest>::Fail(job_already_finished(id));
    }
    return Result<TranscodeRequest>::Fail(job_invalid_transition(id));
}

Result<void> JobStore::mark_running(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (snapshot.state == JobState::queued) {
        snapshot.state = JobState::running;
        touch(snapshot);
        return Result<void>::Ok();
    }
    if (snapshot.state == JobState::canceling) {
        return Result<void>::Fail(job_canceled());
    }
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    return Result<void>::Fail(job_invalid_transition(id));
}

Result<void> JobStore::update_progress(const std::string& id, const JobProgress& progress) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    if (snapshot.state != JobState::running && snapshot.state != JobState::canceling) {
        return Result<void>::Fail(job_invalid_transition(id));
    }
    snapshot.progress = progress;
    touch(snapshot);
    return Result<void>::Ok();
}

Result<void> JobStore::add_warning(const std::string& id, const std::string& warning) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    if (snapshot.state != JobState::running && snapshot.state != JobState::canceling) {
        return Result<void>::Fail(job_invalid_transition(id));
    }
    if (!warning.empty()) {
        snapshot.warnings.push_back(warning);
        touch(snapshot);
    }
    return Result<void>::Ok();
}

Result<void> JobStore::mark_succeeded(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (snapshot.state == JobState::running) {
        snapshot.state = JobState::succeeded;
        snapshot.progress.progress = 1.0;
        touch(snapshot);
        return Result<void>::Ok();
    }
    if (snapshot.state == JobState::canceling) {
        const auto error = job_canceled();
        snapshot.state = JobState::canceled;
        set_error(snapshot, error);
        touch(snapshot);
        return Result<void>::Fail(error);
    }
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    return Result<void>::Fail(job_invalid_transition(id));
}

Result<void> JobStore::mark_failed(const std::string& id, const Error& error) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    if (snapshot.state != JobState::running && snapshot.state != JobState::canceling) {
        return Result<void>::Fail(job_invalid_transition(id));
    }
    snapshot.state = error.code == "job_canceled" ? JobState::canceled : JobState::failed;
    set_error(snapshot, error);
    touch(snapshot);
    return Result<void>::Ok();
}

Result<void> JobStore::request_cancel(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    auto& snapshot = it->second.snapshot;
    if (is_terminal(snapshot.state)) {
        return Result<void>::Fail(job_already_finished(id));
    }
    if (snapshot.state == JobState::queued || snapshot.state == JobState::running) {
        snapshot.state = JobState::canceling;
        touch(snapshot);
    }
    return Result<void>::Ok();
}

bool JobStore::is_cancel_requested(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    return it != jobs_.end() && it->second.snapshot.state == JobState::canceling;
}

} // namespace vsr
