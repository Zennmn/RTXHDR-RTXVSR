#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "platform/logging.h"
#include "video/fake_pipeline.h"
#if defined(VSR_ENABLE_FFMPEG)
#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"
#endif
#if defined(VSR_ENABLE_FFMPEG) && defined(VSR_ENABLE_RTX_SDK)
#include "video/rtx/rtx_dx11_processor.h"
#endif

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>

namespace {

struct CliOptions {
    int port = 49321;
    std::string auth_token;
};

CliOptions parse_cli_options(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            options.port = std::stoi(argv[++i]);
        } else if (arg == "--auth-token" && i + 1 < argc) {
            options.auth_token = argv[++i];
        }
    }
    return options;
}

std::filesystem::path default_log_directory() {
#if defined(_WIN32)
    if (const char* local_app_data = std::getenv("LOCALAPPDATA"); local_app_data != nullptr && *local_app_data != '\0') {
        return std::filesystem::path(local_app_data) / "VSR" / "logs";
    }
#endif

    std::error_code ec;
    const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
    if (!ec && !temp.empty()) {
        return temp / "VSR" / "logs";
    }

    return "logs";
}

} // namespace

int main(int argc, char** argv) {
    CliOptions cli;
    try {
        cli = parse_cli_options(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Invalid command line: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    const int port = cli.port;

    vsr::initialize_logging(default_log_directory());
    vsr::log_info("Starting vsr_backend on 127.0.0.1:" + std::to_string(port));
    vsr::log_info("Log file: " + vsr::current_log_path().string());

    vsr::JobStore store;
#if defined(VSR_ENABLE_FFMPEG)
#if defined(VSR_ENABLE_RTX_SDK)
    std::unique_ptr<vsr::RtxProcessor> rtx = std::make_unique<vsr::RtxDx11Processor>();
#else
    std::unique_ptr<vsr::RtxProcessor> rtx;
#endif
    vsr::JobRunner runner(store, std::make_unique<vsr::FfmpegTranscodePipeline>(std::move(rtx)));
#else
    vsr::JobRunner runner(store, std::make_unique<vsr::FakePipeline>(vsr::FakePipelineMode::succeeds));
#endif
    vsr::HttpServerOptions server_options;
    server_options.auth_token = cli.auth_token;
    server_options.allowed_origins = {
        "http://127.0.0.1:3000",
        "http://localhost:3000",
        "tauri://localhost",
        "http://tauri.localhost",
        "https://tauri.localhost",
    };
    vsr::HttpServer server(store, runner, std::move(server_options));

    if (!server.listen("127.0.0.1", port)) {
        vsr::log_error("Failed to listen on 127.0.0.1:" + std::to_string(port));
        std::cerr << "Failed to listen on 127.0.0.1:" << port << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
