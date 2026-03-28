#include "rtx/core/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace rtx {

namespace {

std::uint64_t NowUnixMilliseconds() {
  const auto now = std::chrono::system_clock::now();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

}  // namespace

Logger::Logger(LogLevel level) : level_(level) {}

void Logger::SetLevel(LogLevel level) {
  std::scoped_lock lock(mutex_);
  level_ = level;
}

LogLevel Logger::level() const {
  std::scoped_lock lock(mutex_);
  return level_;
}

void Logger::SetSink(LogSink sink) {
  std::scoped_lock lock(mutex_);
  sink_ = std::move(sink);
}

void Logger::EnableConsoleOutput(bool enabled) {
  std::scoped_lock lock(mutex_);
  console_output_ = enabled;
}

void Logger::Error(const std::string& message) { Log(LogLevel::kError, message); }
void Logger::Warn(const std::string& message) { Log(LogLevel::kWarn, message); }
void Logger::Info(const std::string& message) { Log(LogLevel::kInfo, message); }
void Logger::Debug(const std::string& message) { Log(LogLevel::kDebug, message); }

void Logger::Log(LogLevel level, const std::string& message) {
  std::scoped_lock lock(mutex_);
  if (static_cast<int>(level) > static_cast<int>(level_)) {
    return;
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &timestamp);
#else
  localtime_r(&timestamp, &local_time);
#endif

  std::ostringstream line;
  line << std::put_time(&local_time, "%H:%M:%S") << " [" << ToLabel(level) << "] " << message;

  LogRecord record;
  record.level = level;
  record.timestamp_ms = NowUnixMilliseconds();
  record.line = line.str();

  if (console_output_) {
    if (level == LogLevel::kError) {
      std::cerr << record.line << '\n';
    } else {
      std::cout << record.line << '\n';
    }
  }

  if (sink_) {
    sink_(record);
  }
}

const char* Logger::ToLabel(LogLevel level) {
  switch (level) {
    case LogLevel::kError:
      return "error";
    case LogLevel::kWarn:
      return "warn";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kDebug:
      return "debug";
  }
  return "log";
}

}  // namespace rtx

