#pragma once

#include <string>
#include <utility>

namespace rtx {

enum class StatusCode {
  kOk = 0,
  kInvalidArgument = 1,
  kNotFound = 2,
  kUnavailable = 3,
  kDependencyMissing = 4,
  kUnsupported = 5,
  kIoError = 6,
  kInternal = 7,
  kCancelled = 8,
};

class Status {
 public:
  Status() = default;
  Status(StatusCode code, std::string message) : code_(code), message_(std::move(message)) {}

  static Status Ok() { return Status(); }
  static Status InvalidArgument(std::string message) {
    return Status(StatusCode::kInvalidArgument, std::move(message));
  }
  static Status NotFound(std::string message) {
    return Status(StatusCode::kNotFound, std::move(message));
  }
  static Status Unavailable(std::string message) {
    return Status(StatusCode::kUnavailable, std::move(message));
  }
  static Status DependencyMissing(std::string message) {
    return Status(StatusCode::kDependencyMissing, std::move(message));
  }
  static Status Unsupported(std::string message) {
    return Status(StatusCode::kUnsupported, std::move(message));
  }
  static Status IoError(std::string message) {
    return Status(StatusCode::kIoError, std::move(message));
  }
  static Status Internal(std::string message) {
    return Status(StatusCode::kInternal, std::move(message));
  }
  static Status Cancelled(std::string message) {
    return Status(StatusCode::kCancelled, std::move(message));
  }

  [[nodiscard]] bool ok() const { return code_ == StatusCode::kOk; }
  [[nodiscard]] StatusCode code() const { return code_; }
  [[nodiscard]] const std::string& message() const { return message_; }

 private:
  StatusCode code_ = StatusCode::kOk;
  std::string message_;
};

}  // namespace rtx

