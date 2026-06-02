#pragma once

#include <filesystem>
#include <string>

namespace vsr {

void initialize_logging(const std::filesystem::path& directory);
void log_info(const std::string& message);
void log_error(const std::string& message);
std::filesystem::path current_log_path();

} // namespace vsr
