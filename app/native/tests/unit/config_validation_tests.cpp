#include <stdexcept>

#include "rtx/core/types.h"
#include "rtx/core/validation.h"

int main() {
  rtx::JobConfig config;
  config.input_path = "input.mp4";
  config.output_path = "output.mkv";

  const auto status = rtx::ValidateJobConfig(config);
  if (!status.ok()) {
    throw std::runtime_error("default config should validate");
  }

  config.output_path = "input.mp4";
  const auto same_path = rtx::ValidateJobConfig(config);
  if (same_path.ok()) {
    throw std::runtime_error("same input/output path should fail");
  }

  return 0;
}

