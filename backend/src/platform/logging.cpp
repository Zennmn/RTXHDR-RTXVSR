#include "platform/logging.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <system_error>

namespace vsr {
namespace {

std::mutex g_log_mutex;
std::filesystem::path g_log_path;

bool configure_log_path_locked(const std::filesystem::path& directory, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = "failed to create log directory '" + directory.string() + "': " + ec.message();
        return false;
    }

    const std::filesystem::path candidate = directory / "vsr_backend.log";
    std::ofstream file(candidate, std::ios::app);
    if (!file) {
        error = "failed to open log file '" + candidate.string() + "'";
        return false;
    }

    g_log_path = candidate;
    return true;
}

std::filesystem::path fallback_log_directory() {
    std::error_code ec;
    const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
    if (!ec && !temp.empty()) {
        return temp / "VSR" / "logs";
    }

    return "logs";
}

void write_line(const std::string& level, const std::string& message) {
    std::lock_guard lock(g_log_mutex);
    if (g_log_path.empty()) {
        std::cerr << level << " " << message << "\n";
        return;
    }

    std::ofstream file(g_log_path, std::ios::app);
    if (file) {
        file << level << " " << message << "\n";
        return;
    }

    std::cerr << level << " " << message << "\n";
    std::cerr << "ERROR failed to write log file '" << g_log_path.string() << "'\n";
}

} // namespace

void initialize_logging(const std::filesystem::path& directory) {
    std::lock_guard lock(g_log_mutex);

    std::string error;
    if (configure_log_path_locked(directory, error)) {
        return;
    }

    std::cerr << "ERROR " << error << "\n";
    const std::filesystem::path fallback = fallback_log_directory();
    if (configure_log_path_locked(fallback, error)) {
        std::cerr << "INFO using fallback log file '" << g_log_path.string() << "'\n";
        return;
    }

    std::cerr << "ERROR " << error << "\n";
    std::cerr << "ERROR file logging is disabled\n";
    g_log_path.clear();
}

void log_info(const std::string& message) {
    write_line("INFO", message);
}

void log_error(const std::string& message) {
    write_line("ERROR", message);
}

std::filesystem::path current_log_path() {
    std::lock_guard lock(g_log_mutex);
    return g_log_path;
}

} // namespace vsr
