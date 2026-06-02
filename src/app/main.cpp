#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"
#if defined(VSR_ENABLE_FFMPEG)
#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"
#endif
#if defined(VSR_ENABLE_FFMPEG) && defined(VSR_ENABLE_RTX_SDK)
#include "video/rtx/rtx_dx11_processor.h"
#endif

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

int parse_port(int argc, char** argv) {
    constexpr int default_port = 49321;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            return std::stoi(argv[++i]);
        }
    }

    return default_port;
}

} // namespace

int main(int argc, char** argv) {
    int port = 49321;
    try {
        port = parse_port(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Invalid --port: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

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
    vsr::HttpServer server(store, runner);

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to listen on 127.0.0.1:" << port << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
