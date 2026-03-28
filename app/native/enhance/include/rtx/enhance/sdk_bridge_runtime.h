#pragma once

#include <exception>

namespace rtx {

class SdkBridgeFatalError final : public std::exception {
 public:
  const char* what() const noexcept override {
    return "RTX Video SDK sample bridge aborted";
  }
};

}  // namespace rtx

