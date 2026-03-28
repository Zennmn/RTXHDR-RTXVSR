#pragma once

#include "rtx/core/status.h"
#include "rtx/core/types.h"

namespace rtx {

RuntimePaths ResolveRuntimePaths();
Status EnsureProcessRuntimePath(const RuntimePaths& paths);

}  // namespace rtx

