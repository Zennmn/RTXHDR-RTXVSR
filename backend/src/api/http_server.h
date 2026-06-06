#pragma once

#include "jobs/job_runner.h"
#include "jobs/job_store.h"

#include <httplib.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vsr {

struct HttpServerOptions {
    std::string auth_token;
    std::vector<std::string> allowed_origins;
};

class HttpServer {
public:
    HttpServer(JobStore& store, JobRunner& runner, HttpServerOptions options = {});
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool listen(const std::string& host, int port);
    void stop();

private:
    bool is_origin_allowed(const httplib::Request& request) const;
    bool authorize_private_request(const httplib::Request& request, httplib::Response& response) const;
    void apply_cors_headers(const httplib::Request& request, httplib::Response& response) const;
    void bind_routes();
    void worker_loop();
    void cancel_queued_jobs(std::deque<std::string> ids);

    JobStore& store_;
    JobRunner& runner_;
    HttpServerOptions options_;
    httplib::Server server_;
    std::mutex worker_mutex_;
    std::condition_variable worker_cv_;
    std::deque<std::string> pending_jobs_;
    std::thread worker_thread_;
    std::string current_job_;
    bool stopping_ = false;
};

} // namespace vsr
