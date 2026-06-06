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
    HttpServerOptions options;
    options.allowed_origins = {"http://127.0.0.1:3000"};

    TestHttpServer server(std::move(options));
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
    EXPECT_EQ(origin->second, "http://127.0.0.1:3000");
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

TEST(HttpServer, rejectsPrivateEndpointWithoutTokenWhenConfigured) {
    HttpServerOptions options;
    options.auth_token = "test-token";
    options.allowed_origins = {"http://127.0.0.1:3000"};

    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/media/probe", nlohmann::json{{"inputPath", "x"}}.dump(), "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);
    EXPECT_NE(response->body.find("unauthorized"), std::string::npos);
}

TEST(HttpServer, acceptsPrivateEndpointWithTokenWhenConfigured) {
    const auto path = std::filesystem::temp_directory_path() / "vsr-auth-probe.bin";
    {
        std::ofstream output(path, std::ios::binary);
        output << "abcdef";
    }

    HttpServerOptions options;
    options.auth_token = "test-token";
    options.allowed_origins = {"http://127.0.0.1:3000"};

    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());
    httplib::Headers headers{{"X-VSR-Token", "test-token"}};

    const auto response = client.Post("/api/media/probe", headers, nlohmann::json{{"inputPath", path.string()}}.dump(), "application/json");

    std::filesystem::remove(path);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
}

TEST(HttpServer, doesNotAllowWildcardCorsForUntrustedOrigins) {
    HttpServerOptions options;
    options.auth_token = "test-token";
    options.allowed_origins = {"http://127.0.0.1:3000"};

    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());
    httplib::Headers headers{
        {"Origin", "https://example.invalid"},
        {"Access-Control-Request-Method", "POST"},
        {"Access-Control-Request-Headers", "Content-Type, X-VSR-Token"},
    };

    const auto response = client.Options("/api/jobs", headers);

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 403);
    EXPECT_EQ(response->headers.find("Access-Control-Allow-Origin"), response->headers.end());
}

TEST(HttpServer, allowsConfiguredCorsOriginAndTokenHeader) {
    HttpServerOptions options;
    options.auth_token = "test-token";
    options.allowed_origins = {"http://127.0.0.1:3000"};

    TestHttpServer server(std::move(options));
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());
    httplib::Headers headers{
        {"Origin", "http://127.0.0.1:3000"},
        {"Access-Control-Request-Method", "POST"},
        {"Access-Control-Request-Headers", "Content-Type, X-VSR-Token"},
    };

    const auto response = client.Options("/api/jobs", headers);

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 204);
    EXPECT_EQ(response->get_header_value("Access-Control-Allow-Origin"), "http://127.0.0.1:3000");
    EXPECT_NE(response->get_header_value("Access-Control-Allow-Headers").find("X-VSR-Token"), std::string::npos);
}
