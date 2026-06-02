#pragma once

#include "jobs/job_runner.h"
#include "jobs/job_store.h"

#include <httplib.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
    void run_job_background(std::string id);

    JobStore& store_;
    JobRunner& runner_;
    httplib::Server server_;
    std::mutex threads_mutex_;
    std::vector<std::thread> job_threads_;
};

} // namespace vsr
