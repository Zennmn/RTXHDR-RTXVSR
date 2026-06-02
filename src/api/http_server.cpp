#include "api/http_server.h"

#include "api/json_dto.h"

#include <nlohmann/json.hpp>

namespace vsr {

namespace {

void set_json(httplib::Response& response, const nlohmann::json& json) {
    response.set_content(json.dump(), "application/json");
}

int status_for_cancel_error(const Error& error) {
    if (error.code == "job_not_found") {
        return 404;
    }
    if (error.code == "job_already_finished") {
        return 409;
    }
    return 400;
}

} // namespace

HttpServer::HttpServer(JobStore& store, JobRunner& runner) : store_(store), runner_(runner) {
    bind_routes();
    worker_thread_ = std::thread([this] { worker_loop(); });
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::listen(const std::string& host, int port) {
    return server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();

    std::string current_job;
    std::deque<std::string> queued_jobs;
    {
        std::lock_guard lock(worker_mutex_);
        if (stopping_) {
            current_job = current_job_;
        } else {
            stopping_ = true;
            current_job = current_job_;
            queued_jobs.swap(pending_jobs_);
        }
    }

    if (!current_job.empty()) {
        const auto canceled = runner_.request_cancel(current_job);
        (void)canceled;
    }
    cancel_queued_jobs(std::move(queued_jobs));

    worker_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void HttpServer::bind_routes() {
    server_.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        set_json(response, {{"version", "0.1.0"}, {"ready", true}});
    });

    server_.Post("/api/jobs", [this](const httplib::Request& request, httplib::Response& response) {
        const auto body = nlohmann::json::parse(request.body, nullptr, false);
        if (body.is_discarded()) {
            response.status = 400;
            set_json(response, error_to_json({"invalid_json", "Request JSON is invalid.", ""}));
            return;
        }

        const auto parsed = parse_transcode_request(body);
        if (!parsed.ok()) {
            response.status = 400;
            set_json(response, error_to_json(parsed.error()));
            return;
        }

        std::string id;
        {
            std::lock_guard lock(worker_mutex_);
            if (stopping_) {
                response.status = 503;
                set_json(response, error_to_json({"server_stopping", "Server is stopping.", ""}));
                return;
            }

            const auto created = store_.create(parsed.value());
            if (!created.ok()) {
                response.status = 400;
                set_json(response, error_to_json(created.error()));
                return;
            }

            id = created.value();
            pending_jobs_.push_back(id);
        }

        worker_cv_.notify_one();
        response.status = 202;
        set_json(response, {{"id", id}});
    });

    server_.Get(R"(/api/jobs/([^/]+))", [this](const httplib::Request& request, httplib::Response& response) {
        const auto snapshot = store_.get(request.matches[1].str());
        if (!snapshot.ok()) {
            response.status = 404;
            set_json(response, error_to_json(snapshot.error()));
            return;
        }

        set_json(response, job_snapshot_to_json(snapshot.value()));
    });

    server_.Post(R"(/api/jobs/([^/]+)/cancel)", [this](const httplib::Request& request, httplib::Response& response) {
        const auto canceled = runner_.request_cancel(request.matches[1].str());
        if (!canceled.ok()) {
            response.status = status_for_cancel_error(canceled.error());
            set_json(response, error_to_json(canceled.error()));
            return;
        }

        set_json(response, {{"accepted", true}});
    });
}

void HttpServer::worker_loop() {
    for (;;) {
        std::string id;
        {
            std::unique_lock lock(worker_mutex_);
            worker_cv_.wait(lock, [this] {
                return stopping_ || !pending_jobs_.empty();
            });

            if (stopping_) {
                return;
            }

            id = std::move(pending_jobs_.front());
            pending_jobs_.pop_front();
            current_job_ = id;
        }

        const auto result = runner_.run_one(id);
        (void)result;

        {
            std::lock_guard lock(worker_mutex_);
            if (current_job_ == id) {
                current_job_.clear();
            }
        }
    }
}

void HttpServer::cancel_queued_jobs(std::deque<std::string> ids) {
    for (const auto& id : ids) {
        const auto canceled = runner_.request_cancel(id);
        if (canceled.ok()) {
            const auto marked = store_.start(id);
            (void)marked;
        }
    }
}

} // namespace vsr
