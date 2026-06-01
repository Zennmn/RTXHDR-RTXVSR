#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vsr {

class JobStore {
public:
    Result<std::string> create(const TranscodeRequest& request);
    Result<JobSnapshot> get(const std::string& id) const;
    Result<TranscodeRequest> get_request(const std::string& id) const;
    Result<JobRecord> get_record(const std::string& id) const;
    Result<void> mark_running(const std::string& id);
    Result<void> update_progress(const std::string& id, const JobProgress& progress);
    Result<void> mark_succeeded(const std::string& id);
    Result<void> mark_failed(const std::string& id, const Error& error);
    Result<void> request_cancel(const std::string& id);
    bool is_cancel_requested(const std::string& id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, JobRecord> jobs_;
    std::uint64_t next_id_ = 1;
};

} // namespace vsr
