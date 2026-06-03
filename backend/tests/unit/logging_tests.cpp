#include "platform/logging.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

std::filesystem::path unique_test_path(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("vsr_logging_tests_" + std::to_string(stamp) + "_" + name);
}

} // namespace

TEST(Logging, initializesWritableDirectory) {
    const auto directory = unique_test_path("writable");
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    vsr::initialize_logging(directory);
    vsr::log_info("unit test log line");

    const auto log_path = vsr::current_log_path();
    EXPECT_EQ(log_path, directory / "vsr_backend.log");
    EXPECT_TRUE(std::filesystem::exists(log_path));

    std::filesystem::remove_all(directory, ignored);
}

TEST(Logging, doesNotSwitchToUnusableDirectory) {
    const auto file_path = unique_test_path("not_a_directory");
    std::error_code ignored;
    std::filesystem::remove(file_path, ignored);
    {
        std::ofstream file(file_path);
        ASSERT_TRUE(file);
    }

    const auto unusable_log_path = file_path / "vsr_backend.log";

    vsr::initialize_logging(file_path);
    vsr::log_error("unit test fallback log line");

    EXPECT_NE(vsr::current_log_path(), unusable_log_path);
    EXPECT_FALSE(std::filesystem::exists(unusable_log_path));

    std::filesystem::remove(file_path, ignored);
}
