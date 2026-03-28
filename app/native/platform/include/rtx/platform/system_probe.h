#pragma once

#include "rtx/core/logger.h"
#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

Status ProbeCurrentSystem(Logger& logger, SystemProbe& probe);

}  // namespace rtx

