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
}

HttpServer::~HttpServer() {
    stop();

    std::vector<std::thread> threads;
    {
        std::lock_guard lock(threads_mutex_);
        threads.swap(job_threads_);
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool HttpServer::listen(const std::string& host, int port) {
    return server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
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

        const auto created = store_.create(parsed.value());
        if (!created.ok()) {
            response.status = 400;
            set_json(response, error_to_json(created.error()));
            return;
        }

        const auto id = created.value();
        run_job_background(id);
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
        const auto canceled = store_.request_cancel(request.matches[1].str());
        if (!canceled.ok()) {
            response.status = status_for_cancel_error(canceled.error());
            set_json(response, error_to_json(canceled.error()));
            return;
        }

        set_json(response, {{"accepted", true}});
    });
}

void HttpServer::run_job_background(std::string id) {
    std::lock_guard lock(threads_mutex_);
    job_threads_.emplace_back([this, id = std::move(id)] {
        const auto result = runner_.run_one(id);
        (void)result;
    });
}

} // namespace vsr
