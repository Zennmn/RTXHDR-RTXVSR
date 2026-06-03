#pragma once

#include "jobs/job_runner.h"
#include "jobs/job_store.h"

#include <httplib.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace vsr {

class HttpServer {
public:
    HttpServer(JobStore& store, JobRunner& runner);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool listen(const std::string& host, int port);
    void stop();

private:
    void bind_routes();
    void worker_loop();
    void cancel_queued_jobs(std::deque<std::string> ids);

    JobStore& store_;
    JobRunner& runner_;
    httplib::Server server_;
    std::mutex worker_mutex_;
    std::condition_variable worker_cv_;
    std::deque<std::string> pending_jobs_;
    std::thread worker_thread_;
    std::string current_job_;
    bool stopping_ = false;
};

} // namespace vsr
