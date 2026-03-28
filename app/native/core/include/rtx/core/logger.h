#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace rtx {

enum class LogLevel {
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
};

struct LogRecord {
  LogLevel level = LogLevel::kInfo;
  std::uint64_t timestamp_ms = 0;
  std::string line;
};

class Logger {
 public:
  using LogSink = std::function<void(const LogRecord&)>;

  explicit Logger(LogLevel level = LogLevel::kInfo);

  void SetLevel(LogLevel level);
  [[nodiscard]] LogLevel level() const;
  void SetSink(LogSink sink);
  void EnableConsoleOutput(bool enabled);

  void Error(const std::string& message);
  void Warn(const std::string& message);
  void Info(const std::string& message);
  void Debug(const std::string& message);

 private:
  void Log(LogLevel level, const std::string& message);
  static const char* ToLabel(LogLevel level);

  LogLevel level_;
  LogSink sink_;
  bool console_output_ = false;
  mutable std::mutex mutex_;
};

}  // namespace rtx

