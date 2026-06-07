#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace vsr;

namespace {

#if defined(_WIN32)
class WsaSession {
public:
    WsaSession() {
        WSADATA data{};
        initialized_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }

    ~WsaSession() {
        if (initialized_) {
            WSACleanup();
        }
    }

    bool initialized() const { return initialized_; }

private:
    bool initialized_ = false;
};
#endif

int reserve_unused_local_port() {
#if defined(_WIN32)
    WsaSession session;
    if (!session.initialized()) {
        return 0;
    }
#endif

    const auto socket_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        return 0;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
#if defined(_WIN32)
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
        return 0;
    }

    socklen_t size = sizeof(address);
    if (::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
#if defined(_WIN32)
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
        return 0;
    }

    const auto port = static_cast<int>(ntohs(address.sin_port));
#if defined(_WIN32)
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
    return port;
}

class TestHttpServer {
public:
    explicit TestHttpServer(HttpServerOptions options = {})
        : port_(reserve_unused_local_port()),
          runner_(store_, std::make_unique<FakePipeline>(FakePipelineMode::succeeds)),
          server_(store_, runner_, std::move(options)) {}

    ~TestHttpServer() {
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    int port() const { return port_; }

    bool start() {
        if (port_ == 0) {
            return false;
        }

        thread_ = std::thread([this] {
            server_.listen("127.0.0.1", port_);
        });

        httplib::Client client("127.0.0.1", port_);
        for (int i = 0; i < 40; ++i) {
            if (const auto response = client.Get("/api/health"); response && response->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

private:
    int port_;
    JobStore store_;
    JobRunner runner_;
    HttpServer server_;
    std::thread thread_;
};

} // namespace

TEST(HttpServer, respondsToCorsPreflight) {
    TestHttpServer server;
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());
    httplib::Headers headers{
        {"Origin", "http://127.0.0.1:3000"},
        {"Access-Control-Request-Method", "POST"},
    };

    const auto response = client.Options("/api/jobs", headers);

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 204);
    const auto origin = response->headers.find("Access-Control-Allow-Origin");
    ASSERT_NE(origin, response->headers.end());
    EXPECT_EQ(origin->second, "*");
}

TEST(HttpServer, healthIncludesAppSessionIdWhenConfigured) {
    HttpServerOptions options;
    options.app_session_id = "test-session-123";
    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Get("/api/health");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    const auto body = nlohmann::json::parse(response->body);
    EXPECT_EQ(body.at("version"), "0.1.1");
    EXPECT_EQ(body.at("ready"), true);
    EXPECT_EQ(body.at("appSessionId"), "test-session-123");
}

TEST(HttpServer, healthOmitsAppSessionIdWhenNotConfigured) {
    TestHttpServer server;
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Get("/api/health");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    const auto body = nlohmann::json::parse(response->body);
    EXPECT_FALSE(body.contains("appSessionId"));
}

TEST(HttpServer, appShutdownUnavailableWithoutAppSession) {
    TestHttpServer server;
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/app/shutdown",
                                      nlohmann::json{{"appSessionId", "test-session-123"}}.dump(),
                                      "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 404);
    EXPECT_NE(response->body.find("app_shutdown_unavailable"), std::string::npos);
}

TEST(HttpServer, appShutdownRejectsInvalidJson) {
    HttpServerOptions options;
    options.app_session_id = "test-session-123";
    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/app/shutdown", "{", "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_NE(response->body.find("invalid_json"), std::string::npos);
}

TEST(HttpServer, appShutdownRejectsMismatchedAppSession) {
    HttpServerOptions options;
    options.app_session_id = "test-session-123";
    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/app/shutdown",
                                      nlohmann::json{{"appSessionId", "wrong-session"}}.dump(),
                                      "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 403);
    EXPECT_NE(response->body.find("app_session_mismatch"), std::string::npos);
}

TEST(HttpServer, rejectsInvalidProbeJson) {
    TestHttpServer server;
    ASSERT_TRUE(server.start());
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
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());
    const std::string body = nlohmann::json{{"inputPath", path.string()}}.dump();

    const auto response = client.Post("/api/media/probe", body, "application/json");

    std::filesystem::remove(path);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    EXPECT_NE(response->body.find("\"name\":\"vsr-http-probe.bin\""), std::string::npos);
    EXPECT_NE(response->body.find("\"sizeBytes\":6"), std::string::npos);
}
