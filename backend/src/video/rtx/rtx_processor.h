#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <cstdint>

struct ID3D11Device;
struct ID3D11Texture2D;

namespace vsr {

struct RtxDx11Frame {
    ID3D11Texture2D* input = nullptr;
    ID3D11Texture2D* output = nullptr;
    std::uint32_t input_width = 0;
    std::uint32_t input_height = 0;
    std::uint32_t output_width = 0;
    std::uint32_t output_height = 0;
};

class RtxProcessor {
public:
    virtual ~RtxProcessor() = default;

    virtual Result<void> initialize(ID3D11Device* device, const ProcessingSettings& settings) = 0;
    virtual Result<void> process(const RtxDx11Frame& frame, const ProcessingSettings& settings) = 0;
    virtual void shutdown() = 0;
};

} // namespace vsr
