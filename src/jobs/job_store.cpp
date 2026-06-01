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

Result<void> JobStore::mark_running(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    it->second.snapshot.state = JobState::running;
    it->second.snapshot.updated_at = std::chrono::system_clock::now();
    return Result<void>::Ok();
}

Result<void> JobStore::update_progress(const std::string& id, const JobProgress& progress) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    it->second.snapshot.progress = progress;
    it->second.snapshot.updated_at = std::chrono::system_clock::now();
    return Result<void>::Ok();
}

Result<void> JobStore::mark_succeeded(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    it->second.snapshot.state = JobState::succeeded;
    it->second.snapshot.progress.progress = 1.0;
    it->second.snapshot.updated_at = std::chrono::system_clock::now();
    return Result<void>::Ok();
}

Result<void> JobStore::mark_failed(const std::string& id, const Error& error) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    it->second.snapshot.state = error.code == "job_canceled" ? JobState::canceled : JobState::failed;
    it->second.snapshot.error_code = error.code;
    it->second.snapshot.error_message = error.message;
    it->second.snapshot.error_details = error.details;
    it->second.snapshot.updated_at = std::chrono::system_clock::now();
    return Result<void>::Ok();
}

Result<void> JobStore::request_cancel(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<void>::Fail(job_not_found(id));
    }
    it->second.snapshot.state = JobState::canceling;
    it->second.snapshot.updated_at = std::chrono::system_clock::now();
    return Result<void>::Ok();
}

bool JobStore::is_cancel_requested(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    return it != jobs_.end() && it->second.snapshot.state == JobState::canceling;
}

} // namespace vsr
