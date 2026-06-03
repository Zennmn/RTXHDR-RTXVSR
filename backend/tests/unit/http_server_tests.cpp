#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

using namespace vsr;

namespace {

class TestHttpServer {
public:
    TestHttpServer()
        : port_(next_port_.fetch_add(1)),
          runner_(store_, std::make_unique<FakePipeline>(FakePipelineMode::succeeds)),
          server_(store_, runner_) {}

    ~TestHttpServer() {
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    int port() const { return port_; }

    void start() {
        thread_ = std::thread([this] {
            server_.listen("127.0.0.1", port_);
        });

        httplib::Client client("127.0.0.1", port_);
        for (int i = 0; i < 40; ++i) {
            if (const auto response = client.Get("/api/health"); response && response->status == 200) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        FAIL() << "test server did not become ready";
    }

private:
    static std::atomic_int next_port_;
    int port_;
    JobStore store_;
    JobRunner runner_;
    HttpServer server_;
    std::thread thread_;
};

std::atomic_int TestHttpServer::next_port_{49322};

} // namespace

TEST(HttpServer, respondsToCorsPreflight) {
    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Options("/api/jobs");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 204);
    const auto origin = response->headers.find("Access-Control-Allow-Origin");
    ASSERT_NE(origin, response->headers.end());
    EXPECT_EQ(origin->second, "*");
}

TEST(HttpServer, rejectsInvalidProbeJson) {
    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/media/probe", "{", "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_NE(response->body.find("invalid_json"), std::string::npos);
}

TEST(HttpServer, probesExistingFileWithFallbackMetadata) {
    const auto path = std::filesystem::temp_directory_path() / "vsr-http-probe.bin";
    {
        std::ofstream output(path, std::ios::binary);
        output << "abcdef";
    }

    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());
    const std::string body = std::string("{\"inputPath\":\"") + path.string() + "\"}";

    const auto response = client.Post("/api/media/probe", body, "application/json");

    std::filesystem::remove(path);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    EXPECT_NE(response->body.find("\"name\":\"vsr-http-probe.bin\""), std::string::npos);
    EXPECT_NE(response->body.find("\"sizeBytes\":6"), std::string::npos);
}
