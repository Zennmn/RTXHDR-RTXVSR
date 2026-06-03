#pragma once

#include "video/rtx/rtx_processor.h"

namespace vsr {

class RtxDx11Processor final : public RtxProcessor {
public:
    RtxDx11Processor() = default;
    ~RtxDx11Processor() override;

    RtxDx11Processor(const RtxDx11Processor&) = delete;
    RtxDx11Processor& operator=(const RtxDx11Processor&) = delete;

    Result<void> initialize(ID3D11Device* device, const ProcessingSettings& settings) override;
    Result<void> process(const RtxDx11Frame& frame, const ProcessingSettings& settings) override;
    void shutdown() override;

private:
    bool initialized_ = false;
    bool initialized_vsr_enabled_ = false;
    bool initialized_hdr_enabled_ = false;
};

} // namespace vsr
