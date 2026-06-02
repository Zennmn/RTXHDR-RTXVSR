#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"

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
    vsr::JobRunner runner(store, std::make_unique<vsr::FakePipeline>(vsr::FakePipelineMode::succeeds));
    vsr::HttpServer server(store, runner);

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to listen on 127.0.0.1:" << port << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
