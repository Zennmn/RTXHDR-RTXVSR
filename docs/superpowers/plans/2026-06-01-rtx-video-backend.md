# RTX Video Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows-only local backend service that transcodes complete video files through NVIDIA RTX Video VSR and TrueHDR, exposing a small JSON API for a future Tauri UI.

**Architecture:** Create one native C++20 executable, `vsr_backend.exe`, with a localhost HTTP API, a single-worker job runner, and a video pipeline split behind interfaces so unit tests can run without RTX hardware. The hardware path uses FFmpeg for demux/decode/encode/mux, D3D11 hardware frames for low-copy Windows processing, and the local RTX Video SDK sample API for VSR/TrueHDR evaluation.

**Tech Stack:** C++20, CMake, Windows 10/11, D3D11, NVIDIA RTX Video SDK 1.1.0, FFmpeg libraries with NVIDIA hardware acceleration, `cpp-httplib`, `nlohmann/json`, GoogleTest.

---

## Scope Check

This plan implements the first backend slice only: a local sidecar service, single active transcode job, MP4 output, VSR and TrueHDR processing, status polling, cancellation, capability detection, and frontend integration docs. It does not implement a frontend, MKV, AV1, multi-GPU scheduling, batch UI, preview UI, or remote networking.

The existing SDK tree under `RTX_Video_SDK_v1.1.0` is treated as a local dependency. Do not modify NVIDIA SDK files unless a task explicitly names them. Do not commit `RTX_Video_SDK_v1.1.0.zip`, `.codegraph/`, build outputs, logs, or generated media.

## File Structure

Create this project structure:

- `CMakeLists.txt`: root build configuration.
- `cmake/FindFFmpeg.cmake`: imports FFmpeg headers and libraries from `FFMPEG_ROOT` or system paths.
- `vcpkg.json`: optional developer dependency manifest for lightweight C++ libraries.
- `.gitignore`: ignore build outputs, logs, generated media, CodeGraph cache, and SDK zip.
- `src/app/main.cpp`: parses CLI args and starts the local API server.
- `src/api/http_server.h`, `src/api/http_server.cpp`: route binding and JSON responses.
- `src/api/json_dto.h`, `src/api/json_dto.cpp`: request/response JSON conversion.
- `src/core/result.h`: small `Result<T>` and `Error` types.
- `src/core/time.h`, `src/core/time.cpp`: timestamps for job snapshots.
- `src/jobs/job_types.h`, `src/jobs/job_store.h`, `src/jobs/job_store.cpp`: job request, snapshot, and thread-safe state storage.
- `src/jobs/job_runner.h`, `src/jobs/job_runner.cpp`: single-worker execution and cancellation.
- `src/video/video_pipeline.h`: testable transcode pipeline interface.
- `src/video/fake_pipeline.h`, `src/video/fake_pipeline.cpp`: deterministic pipeline for unit tests and API development.
- `src/video/ffmpeg/ffmpeg_probe.h`, `src/video/ffmpeg/ffmpeg_probe.cpp`: media probing.
- `src/video/ffmpeg/ffmpeg_transcode_pipeline.h`, `src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`: real video pipeline.
- `src/video/rtx/rtx_processor.h`: RTX abstraction.
- `src/video/rtx/rtx_dx11_processor.h`, `src/video/rtx/rtx_dx11_processor.cpp`: DX11 RTX SDK wrapper.
- `src/platform/capabilities.h`, `src/platform/capabilities.cpp`: runtime capability detection.
- `src/platform/logging.h`, `src/platform/logging.cpp`: simple file logger.
- `docs/frontend-integration.md`: API and UI integration guide.
- `tests/unit/job_request_tests.cpp`: request validation tests.
- `tests/unit/job_store_tests.cpp`: state transition tests.
- `tests/unit/job_runner_tests.cpp`: worker, progress, failure, cancellation tests.
- `tests/unit/json_dto_tests.cpp`: endpoint JSON shape tests.
- `tests/unit/capabilities_tests.cpp`: capability mapping tests using fakes.
- `tests/scripts/hardware_smoke.ps1`: manual Windows RTX smoke test.

## Implementation Conventions

- Keep the backend bound to `127.0.0.1`.
- Use stable JSON field names from the design spec.
- Use one worker thread for MVP.
- Return structured errors with `code`, `message`, and `details`.
- Prefer polling over WebSocket/SSE in MVP to keep dependencies small.
- Unit tests must pass without FFmpeg, RTX SDK, or RTX hardware when `VSR_ENABLE_FFMPEG=OFF` and `VSR_ENABLE_RTX_SDK=OFF`.
- Hardware code must fail with clear errors when dependencies are disabled or unavailable.

---

### Task 1: Bootstrap Build And Dependency Boundaries

**Files:**
- Create: `.gitignore`
- Create: `vcpkg.json`
- Create: `CMakeLists.txt`
- Create: `cmake/FindFFmpeg.cmake`
- Create: `src/core/result.h`
- Test: configure command listed below

- [ ] **Step 1: Write the failing configure check**

Run before creating files:

```powershell
cmake -S . -B build -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
```

Expected: FAIL with a message that the source directory does not contain `CMakeLists.txt`.

- [ ] **Step 2: Create `.gitignore`**

```gitignore
.codegraph/
build/
out/
dist/
logs/
*.log
*.ilk
*.pdb
*.obj
*.exe
*.dll
*.lib
*.mp4
*.mkv
*.mov
*.avi
RTX_Video_SDK_v1.1.0.zip
```

- [ ] **Step 3: Create `vcpkg.json`**

```json
{
  "name": "rtx-video-backend",
  "version-string": "0.1.0",
  "dependencies": [
    "cpp-httplib",
    "nlohmann-json",
    "gtest"
  ]
}
```

- [ ] **Step 4: Create `src/core/result.h`**

```cpp
#pragma once

#include <optional>
#include <string>
#include <utility>

namespace vsr {

struct Error {
    std::string code;
    std::string message;
    std::string details;
};

template <typename T>
class Result {
public:
    static Result Ok(T value) { return Result(std::move(value)); }
    static Result Fail(Error error) { return Result(std::move(error)); }

    bool ok() const { return value_.has_value(); }
    const T& value() const { return *value_; }
    T& value() { return *value_; }
    const Error& error() const { return *error_; }

private:
    explicit Result(T value) : value_(std::move(value)) {}
    explicit Result(Error error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<Error> error_;
};

template <>
class Result<void> {
public:
    static Result Ok() { return Result(); }
    static Result Fail(Error error) { return Result(std::move(error)); }

    bool ok() const { return !error_.has_value(); }
    const Error& error() const { return *error_; }

private:
    Result() = default;
    explicit Result(Error error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

} // namespace vsr
```

- [ ] **Step 5: Create `cmake/FindFFmpeg.cmake`**

```cmake
set(_ffmpeg_roots "")
if(DEFINED FFMPEG_ROOT)
  list(APPEND _ffmpeg_roots "${FFMPEG_ROOT}")
endif()
if(DEFINED ENV{FFMPEG_ROOT})
  list(APPEND _ffmpeg_roots "$ENV{FFMPEG_ROOT}")
endif()

find_path(FFMPEG_INCLUDE_DIR
  NAMES libavformat/avformat.h
  PATHS ${_ffmpeg_roots}
  PATH_SUFFIXES include
)

foreach(_component avformat avcodec avutil swscale swresample)
  string(TOUPPER "${_component}" _upper)
  find_library(FFMPEG_${_upper}_LIBRARY
    NAMES ${_component}
    PATHS ${_ffmpeg_roots}
    PATH_SUFFIXES lib lib/x64 bin
  )
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS
    FFMPEG_INCLUDE_DIR
    FFMPEG_AVFORMAT_LIBRARY
    FFMPEG_AVCODEC_LIBRARY
    FFMPEG_AVUTIL_LIBRARY
    FFMPEG_SWSCALE_LIBRARY
    FFMPEG_SWRESAMPLE_LIBRARY
)

if(FFmpeg_FOUND AND NOT TARGET FFmpeg::FFmpeg)
  add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
  target_include_directories(FFmpeg::FFmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
  target_link_libraries(FFmpeg::FFmpeg INTERFACE
    "${FFMPEG_AVFORMAT_LIBRARY}"
    "${FFMPEG_AVCODEC_LIBRARY}"
    "${FFMPEG_AVUTIL_LIBRARY}"
    "${FFMPEG_SWSCALE_LIBRARY}"
    "${FFMPEG_SWRESAMPLE_LIBRARY}"
  )
endif()
```

- [ ] **Step 6: Create `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)
project(rtx_video_backend LANGUAGES CXX)

option(VSR_ENABLE_TESTS "Build unit tests" ON)
option(VSR_ENABLE_FFMPEG "Build FFmpeg transcode pipeline" ON)
option(VSR_ENABLE_RTX_SDK "Build NVIDIA RTX Video SDK integration" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

add_library(vsr_backend_core
  src/core/time.cpp
  src/jobs/job_store.cpp
  src/jobs/job_runner.cpp
  src/video/fake_pipeline.cpp
  src/api/json_dto.cpp
  src/api/http_server.cpp
  src/platform/capabilities.cpp
  src/platform/logging.cpp
)

target_include_directories(vsr_backend_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(vsr_backend_core PUBLIC nlohmann_json::nlohmann_json httplib::httplib)
target_compile_definitions(vsr_backend_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

if(VSR_ENABLE_FFMPEG)
  find_package(FFmpeg REQUIRED)
  target_sources(vsr_backend_core PRIVATE
    src/video/ffmpeg/ffmpeg_probe.cpp
    src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp
  )
  target_link_libraries(vsr_backend_core PRIVATE FFmpeg::FFmpeg)
  target_compile_definitions(vsr_backend_core PUBLIC VSR_ENABLE_FFMPEG=1)
endif()

set(VSR_RTX_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/RTX_Video_SDK_v1.1.0" CACHE PATH "NVIDIA RTX Video SDK root")
if(VSR_ENABLE_RTX_SDK)
  add_library(rtx_video_api_dx11 STATIC
    RTX_Video_SDK_v1.1.0/samples/RTX_Video_API/rtx_video_api_dx11_impl.cpp
  )
  target_include_directories(rtx_video_api_dx11 PUBLIC
    "${VSR_RTX_SDK_ROOT}/include"
    "${VSR_RTX_SDK_ROOT}/samples/RTX_Video_API"
  )
  target_link_libraries(rtx_video_api_dx11 PUBLIC
    d3d11
    dxgi
    "${VSR_RTX_SDK_ROOT}/lib/Windows/x64/nvsdk_ngx_s.lib"
  )
  target_compile_definitions(rtx_video_api_dx11 PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

  target_sources(vsr_backend_core PRIVATE src/video/rtx/rtx_dx11_processor.cpp)
  target_include_directories(vsr_backend_core PRIVATE "${VSR_RTX_SDK_ROOT}/samples/RTX_Video_API")
  target_link_libraries(vsr_backend_core PRIVATE rtx_video_api_dx11 d3d11 dxgi)
  target_compile_definitions(vsr_backend_core PUBLIC VSR_ENABLE_RTX_SDK=1)
endif()

add_executable(vsr_backend src/app/main.cpp)
target_link_libraries(vsr_backend PRIVATE vsr_backend_core)

if(VSR_ENABLE_RTX_SDK)
  add_custom_command(TARGET vsr_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${VSR_RTX_SDK_ROOT}/bin/Windows/x64/rel/nvngx_vsr.dll"
      "$<TARGET_FILE_DIR:vsr_backend>/nvngx_vsr.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${VSR_RTX_SDK_ROOT}/bin/Windows/x64/rel/nvngx_truehdr.dll"
      "$<TARGET_FILE_DIR:vsr_backend>/nvngx_truehdr.dll"
  )
endif()

if(VSR_ENABLE_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_executable(vsr_backend_tests
    tests/unit/job_request_tests.cpp
    tests/unit/job_store_tests.cpp
    tests/unit/job_runner_tests.cpp
    tests/unit/json_dto_tests.cpp
    tests/unit/capabilities_tests.cpp
  )
  target_link_libraries(vsr_backend_tests PRIVATE vsr_backend_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(vsr_backend_tests)
endif()
```

- [ ] **Step 7: Run configure again**

```powershell
cmake -S . -B build -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
```

Expected: configure reaches dependency discovery. If vcpkg is not configured, it fails on `nlohmann_json` or `httplib`, which proves the project file is now being read.

- [ ] **Step 8: Commit**

```powershell
git add .gitignore vcpkg.json CMakeLists.txt cmake/FindFFmpeg.cmake src/core/result.h
git commit -m "build: bootstrap backend project"
```

---

### Task 2: Domain Types And Request Validation

**Files:**
- Create: `src/jobs/job_types.h`
- Create: `tests/unit/job_request_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing request validation tests**

Create `tests/unit/job_request_tests.cpp`:

```cpp
#include "jobs/job_types.h"

#include <gtest/gtest.h>

using namespace vsr;

TEST(JobRequestValidation, rejectsMissingInputPath) {
    TranscodeRequest request;
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "input_path_required");
}

TEST(JobRequestValidation, rejectsRequestWithoutProcessing) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";

    const auto result = validate_request(request);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "processing_required");
}

TEST(JobRequestValidation, acceptsVsrHdrMp4Request) {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    request.processing.vsr.quality = 3;
    request.processing.vsr.scale = 2.0;
    request.processing.hdr.enabled = true;
    request.output.container = "mp4";
    request.output.video_codec = "hevc";

    const auto result = validate_request(request);

    ASSERT_TRUE(result.ok()) << result.error().message;
}
```

- [ ] **Step 2: Run the tests and verify red**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R JobRequestValidation --output-on-failure
```

Expected: FAIL because `jobs/job_types.h` does not exist.

- [ ] **Step 3: Create `src/jobs/job_types.h`**

```cpp
#pragma once

#include "core/result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace vsr {

struct VsrSettings {
    bool enabled = false;
    int quality = 3;
    double scale = 2.0;
};

struct HdrSettings {
    bool enabled = false;
    int contrast = 100;
    int saturation = 100;
    int middle_gray = 44;
    int max_luminance = 1000;
};

struct ProcessingSettings {
    VsrSettings vsr;
    HdrSettings hdr;
};

struct OutputSettings {
    std::string container = "mp4";
    std::string video_codec = "h264";
    std::string audio_mode = "copy";
    std::string subtitle_mode = "copy-compatible";
};

struct TranscodeRequest {
    std::string input_path;
    std::string output_path;
    ProcessingSettings processing;
    OutputSettings output;
};

enum class JobState {
    queued,
    running,
    succeeded,
    failed,
    canceling,
    canceled
};

enum class JobStage {
    validating,
    probing,
    initializing_gpu,
    decoding,
    processing_rtx,
    encoding,
    muxing,
    finalizing
};

struct JobProgress {
    JobStage stage = JobStage::validating;
    double progress = 0.0;
    std::int64_t frames_done = 0;
    std::int64_t frames_total = 0;
    double fps = 0.0;
    std::int64_t eta_seconds = 0;
};

struct JobSnapshot {
    std::string id;
    JobState state = JobState::queued;
    JobProgress progress;
    std::string input_path;
    std::string output_path;
    std::vector<std::string> warnings;
    std::string error_code;
    std::string error_message;
    std::string error_details;
};

struct CancellationToken {
    std::atomic_bool requested{false};
};

inline const char* to_string(JobState state) {
    switch (state) {
    case JobState::queued: return "queued";
    case JobState::running: return "running";
    case JobState::succeeded: return "succeeded";
    case JobState::failed: return "failed";
    case JobState::canceling: return "canceling";
    case JobState::canceled: return "canceled";
    }
    return "failed";
}

inline const char* to_string(JobStage stage) {
    switch (stage) {
    case JobStage::validating: return "validating";
    case JobStage::probing: return "probing";
    case JobStage::initializing_gpu: return "initializing_gpu";
    case JobStage::decoding: return "decoding";
    case JobStage::processing_rtx: return "processing_rtx";
    case JobStage::encoding: return "encoding";
    case JobStage::muxing: return "muxing";
    case JobStage::finalizing: return "finalizing";
    }
    return "validating";
}

inline Result<void> validate_request(const TranscodeRequest& request) {
    if (request.input_path.empty()) {
        return Result<void>::Fail({"input_path_required", "Input path is required.", ""});
    }
    if (request.output_path.empty()) {
        return Result<void>::Fail({"output_path_required", "Output path is required.", ""});
    }
    if (!request.processing.vsr.enabled && !request.processing.hdr.enabled) {
        return Result<void>::Fail({"processing_required", "Enable VSR, HDR, or both.", ""});
    }
    if (request.output.container != "mp4") {
        return Result<void>::Fail({"unsupported_container", "The first backend release writes MP4 output.", request.output.container});
    }
    if (request.output.video_codec != "h264" && request.output.video_codec != "hevc") {
        return Result<void>::Fail({"unsupported_video_codec", "Use h264 or hevc for MP4 output.", request.output.video_codec});
    }
    if (request.processing.vsr.enabled && (request.processing.vsr.quality < 1 || request.processing.vsr.quality > 4)) {
        return Result<void>::Fail({"invalid_vsr_quality", "VSR quality must be between 1 and 4.", std::to_string(request.processing.vsr.quality)});
    }
    if (request.processing.vsr.enabled && (request.processing.vsr.scale < 1.0 || request.processing.vsr.scale > 4.0)) {
        return Result<void>::Fail({"invalid_vsr_scale", "VSR scale must be between 1.0 and 4.0.", std::to_string(request.processing.vsr.scale)});
    }
    if (request.processing.hdr.enabled && request.output.video_codec != "hevc") {
        return Result<void>::Fail({"hdr_requires_hevc", "HDR output uses HEVC Main10 in the first backend release.", request.output.video_codec});
    }
    return Result<void>::Ok();
}

} // namespace vsr
```

- [ ] **Step 4: Run the validation tests and verify green**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R JobRequestValidation --output-on-failure
```

Expected: PASS for the three `JobRequestValidation` tests.

- [ ] **Step 5: Commit**

```powershell
git add src/jobs/job_types.h tests/unit/job_request_tests.cpp CMakeLists.txt
git commit -m "feat: add transcode request validation"
```

---

### Task 3: Job Store And Fake Pipeline Runner

**Files:**
- Create: `src/jobs/job_store.h`
- Create: `src/jobs/job_store.cpp`
- Create: `src/video/video_pipeline.h`
- Create: `src/video/fake_pipeline.h`
- Create: `src/video/fake_pipeline.cpp`
- Create: `src/jobs/job_runner.h`
- Create: `src/jobs/job_runner.cpp`
- Create: `tests/unit/job_store_tests.cpp`
- Create: `tests/unit/job_runner_tests.cpp`

- [ ] **Step 1: Write failing job store tests**

Create `tests/unit/job_store_tests.cpp`:

```cpp
#include "jobs/job_store.h"

#include <gtest/gtest.h>

using namespace vsr;

TEST(JobStore, createsQueuedJobWithStableSnapshot) {
    JobStore store;
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;

    const auto created = store.create(request);

    ASSERT_TRUE(created.ok());
    const auto snapshot = store.get(created.value());
    ASSERT_TRUE(snapshot.ok());
    EXPECT_EQ(snapshot.value().state, JobState::queued);
    EXPECT_EQ(snapshot.value().input_path, request.input_path);
    EXPECT_EQ(snapshot.value().output_path, request.output_path);
}

TEST(JobStore, updatesProgressAndFailure) {
    JobStore store;
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    const auto id = store.create(request).value();

    JobProgress progress;
    progress.stage = JobStage::encoding;
    progress.progress = 0.5;
    progress.frames_done = 50;
    progress.frames_total = 100;
    store.mark_running(id);
    store.update_progress(id, progress);
    store.mark_failed(id, {"transcode_failed", "Transcode failed.", "unit test"});

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.progress.stage, JobStage::encoding);
    EXPECT_EQ(snapshot.error_code, "transcode_failed");
}
```

- [ ] **Step 2: Write failing runner tests**

Create `tests/unit/job_runner_tests.cpp`:

```cpp
#include "jobs/job_runner.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>
#include <thread>

using namespace vsr;

TEST(JobRunner, completesFakePipelineJob) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::succeeds);
    JobRunner runner(store, std::move(pipeline));

    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    const auto id = store.create(request).value();

    runner.run_one(id);

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::succeeded);
    EXPECT_DOUBLE_EQ(snapshot.progress.progress, 1.0);
}

TEST(JobRunner, recordsFakePipelineFailure) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::fails);
    JobRunner runner(store, std::move(pipeline));

    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    const auto id = store.create(request).value();

    runner.run_one(id);

    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.error_code, "fake_pipeline_failed");
}
```

- [ ] **Step 3: Run the tests and verify red**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R "JobStore|JobRunner" --output-on-failure
```

Expected: FAIL because job store and runner headers do not exist.

- [ ] **Step 4: Create `src/video/video_pipeline.h`**

```cpp
#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <functional>

namespace vsr {

using ProgressCallback = std::function<void(const JobProgress&)>;

class VideoPipeline {
public:
    virtual ~VideoPipeline() = default;
    virtual Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) = 0;
};

} // namespace vsr
```

- [ ] **Step 5: Create fake pipeline files**

`src/video/fake_pipeline.h`:

```cpp
#pragma once

#include "video/video_pipeline.h"

namespace vsr {

enum class FakePipelineMode {
    succeeds,
    fails
};

class FakePipeline final : public VideoPipeline {
public:
    explicit FakePipeline(FakePipelineMode mode);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    FakePipelineMode mode_;
};

} // namespace vsr
```

`src/video/fake_pipeline.cpp`:

```cpp
#include "video/fake_pipeline.h"

#include <thread>

namespace vsr {

FakePipeline::FakePipeline(FakePipelineMode mode) : mode_(mode) {}

Result<void> FakePipeline::run(const TranscodeRequest&, CancellationToken& cancellation, ProgressCallback progress) {
    for (int i = 0; i <= 4; ++i) {
        if (cancellation.requested.load()) {
            return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
        }
        JobProgress snapshot;
        snapshot.stage = i < 2 ? JobStage::decoding : JobStage::encoding;
        snapshot.progress = static_cast<double>(i) / 4.0;
        snapshot.frames_done = i * 25;
        snapshot.frames_total = 100;
        snapshot.fps = 60.0;
        snapshot.eta_seconds = (4 - i);
        progress(snapshot);
    }

    if (mode_ == FakePipelineMode::fails) {
        return Result<void>::Fail({"fake_pipeline_failed", "Fake pipeline failed.", "configured failure"});
    }
    return Result<void>::Ok();
}

} // namespace vsr
```

- [ ] **Step 6: Create job store files**

`src/jobs/job_store.h`:

```cpp
#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace vsr {

class JobStore {
public:
    Result<std::string> create(const TranscodeRequest& request);
    Result<JobSnapshot> get(const std::string& id) const;
    Result<void> mark_running(const std::string& id);
    Result<void> update_progress(const std::string& id, const JobProgress& progress);
    Result<void> mark_succeeded(const std::string& id);
    Result<void> mark_failed(const std::string& id, const Error& error);
    Result<void> request_cancel(const std::string& id);
    bool is_cancel_requested(const std::string& id) const;

private:
    Result<JobSnapshot*> find_mutable(const std::string& id);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, JobSnapshot> jobs_;
    std::uint64_t next_id_ = 1;
};

} // namespace vsr
```

`src/jobs/job_store.cpp`:

```cpp
#include "jobs/job_store.h"

#include <format>

namespace vsr {

Result<std::string> JobStore::create(const TranscodeRequest& request) {
    const auto valid = validate_request(request);
    if (!valid.ok()) {
        return Result<std::string>::Fail(valid.error());
    }

    std::lock_guard lock(mutex_);
    const std::string id = std::format("{:026}", next_id_++);
    JobSnapshot snapshot;
    snapshot.id = id;
    snapshot.state = JobState::queued;
    snapshot.input_path = request.input_path;
    snapshot.output_path = request.output_path;
    jobs_.emplace(id, snapshot);
    return Result<std::string>::Ok(id);
}

Result<JobSnapshot> JobStore::get(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<JobSnapshot>::Fail({"job_not_found", "Job was not found.", id});
    }
    return Result<JobSnapshot>::Ok(it->second);
}

Result<JobSnapshot*> JobStore::find_mutable(const std::string& id) {
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        return Result<JobSnapshot*>::Fail({"job_not_found", "Job was not found.", id});
    }
    return Result<JobSnapshot*>::Ok(&it->second);
}

Result<void> JobStore::mark_running(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto found = find_mutable(id);
    if (!found.ok()) return Result<void>::Fail(found.error());
    found.value()->state = JobState::running;
    return Result<void>::Ok();
}

Result<void> JobStore::update_progress(const std::string& id, const JobProgress& progress) {
    std::lock_guard lock(mutex_);
    auto found = find_mutable(id);
    if (!found.ok()) return Result<void>::Fail(found.error());
    found.value()->progress = progress;
    return Result<void>::Ok();
}

Result<void> JobStore::mark_succeeded(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto found = find_mutable(id);
    if (!found.ok()) return Result<void>::Fail(found.error());
    found.value()->state = JobState::succeeded;
    found.value()->progress.progress = 1.0;
    return Result<void>::Ok();
}

Result<void> JobStore::mark_failed(const std::string& id, const Error& error) {
    std::lock_guard lock(mutex_);
    auto found = find_mutable(id);
    if (!found.ok()) return Result<void>::Fail(found.error());
    found.value()->state = error.code == "job_canceled" ? JobState::canceled : JobState::failed;
    found.value()->error_code = error.code;
    found.value()->error_message = error.message;
    found.value()->error_details = error.details;
    return Result<void>::Ok();
}

Result<void> JobStore::request_cancel(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto found = find_mutable(id);
    if (!found.ok()) return Result<void>::Fail(found.error());
    found.value()->state = JobState::canceling;
    return Result<void>::Ok();
}

bool JobStore::is_cancel_requested(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    return it != jobs_.end() && it->second.state == JobState::canceling;
}

} // namespace vsr
```

- [ ] **Step 7: Create job runner files**

`src/jobs/job_runner.h`:

```cpp
#pragma once

#include "jobs/job_store.h"
#include "video/video_pipeline.h"

#include <memory>

namespace vsr {

class JobRunner {
public:
    JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline);
    void run_one(const std::string& id);

private:
    JobStore& store_;
    std::unique_ptr<VideoPipeline> pipeline_;
};

} // namespace vsr
```

`src/jobs/job_runner.cpp`:

```cpp
#include "jobs/job_runner.h"

namespace vsr {

JobRunner::JobRunner(JobStore& store, std::unique_ptr<VideoPipeline> pipeline)
    : store_(store), pipeline_(std::move(pipeline)) {}

void JobRunner::run_one(const std::string& id) {
    const auto snapshot = store_.get(id);
    if (!snapshot.ok()) {
        return;
    }

    store_.mark_running(id);
    CancellationToken cancellation;
    const auto result = pipeline_->run(
        TranscodeRequest{
            .input_path = snapshot.value().input_path,
            .output_path = snapshot.value().output_path,
            .processing = {},
            .output = {}
        },
        cancellation,
        [this, &id](const JobProgress& progress) {
            store_.update_progress(id, progress);
        });

    if (result.ok()) {
        store_.mark_succeeded(id);
    } else {
        store_.mark_failed(id, result.error());
    }
}

} // namespace vsr
```

- [ ] **Step 8: Run the tests and verify green**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R "JobStore|JobRunner" --output-on-failure
```

Expected: PASS for job store and runner tests.

- [ ] **Step 9: Commit**

```powershell
git add src/jobs src/video/video_pipeline.h src/video/fake_pipeline.* tests/unit/job_store_tests.cpp tests/unit/job_runner_tests.cpp
git commit -m "feat: add job store and runner"
```

---

### Task 4: JSON DTOs And Local HTTP API

**Files:**
- Create: `src/api/json_dto.h`
- Create: `src/api/json_dto.cpp`
- Create: `src/api/http_server.h`
- Create: `src/api/http_server.cpp`
- Create: `tests/unit/json_dto_tests.cpp`
- Create: `src/app/main.cpp`

- [ ] **Step 1: Write failing DTO tests**

Create `tests/unit/json_dto_tests.cpp`:

```cpp
#include "api/json_dto.h"

#include <gtest/gtest.h>

using namespace vsr;

TEST(JsonDto, parsesCreateJobRequest) {
    const auto json = nlohmann::json::parse(R"json({
      "inputPath": "C:\\Videos\\in.mp4",
      "outputPath": "C:\\Videos\\out.mp4",
      "processing": {
        "vsr": { "enabled": true, "quality": 2, "scale": 2.0 },
        "hdr": { "enabled": true, "contrast": 100, "saturation": 100, "middleGray": 44, "maxLuminance": 1000 }
      },
      "output": { "container": "mp4", "videoCodec": "hevc", "audioMode": "copy", "subtitleMode": "copy-compatible" }
    })json");

    const auto parsed = parse_transcode_request(json);

    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_EQ(parsed.value().input_path, "C:\\Videos\\in.mp4");
    EXPECT_TRUE(parsed.value().processing.vsr.enabled);
    EXPECT_TRUE(parsed.value().processing.hdr.enabled);
    EXPECT_EQ(parsed.value().output.video_codec, "hevc");
}

TEST(JsonDto, serializesJobSnapshotForFrontend) {
    JobSnapshot snapshot;
    snapshot.id = "0001";
    snapshot.state = JobState::running;
    snapshot.progress.stage = JobStage::encoding;
    snapshot.progress.progress = 0.5;
    snapshot.frames_total = 0;
    snapshot.input_path = "C:\\Videos\\in.mp4";
    snapshot.output_path = "C:\\Videos\\out.mp4";

    const auto json = job_snapshot_to_json(snapshot);

    EXPECT_EQ(json["id"], "0001");
    EXPECT_EQ(json["state"], "running");
    EXPECT_EQ(json["stage"], "encoding");
    EXPECT_EQ(json["progress"], 0.5);
    EXPECT_EQ(json["inputPath"], "C:\\Videos\\in.mp4");
}
```

- [ ] **Step 2: Run DTO tests and verify red**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R JsonDto --output-on-failure
```

Expected: FAIL because `api/json_dto.h` does not exist.

- [ ] **Step 3: Create JSON DTO files**

`src/api/json_dto.h`:

```cpp
#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <nlohmann/json.hpp>

namespace vsr {

Result<TranscodeRequest> parse_transcode_request(const nlohmann::json& json);
nlohmann::json error_to_json(const Error& error);
nlohmann::json job_snapshot_to_json(const JobSnapshot& snapshot);

} // namespace vsr
```

`src/api/json_dto.cpp`:

```cpp
#include "api/json_dto.h"

namespace vsr {

Result<TranscodeRequest> parse_transcode_request(const nlohmann::json& json) {
    try {
        TranscodeRequest request;
        request.input_path = json.value("inputPath", "");
        request.output_path = json.value("outputPath", "");

        const auto processing = json.value("processing", nlohmann::json::object());
        const auto vsr = processing.value("vsr", nlohmann::json::object());
        request.processing.vsr.enabled = vsr.value("enabled", false);
        request.processing.vsr.quality = vsr.value("quality", 3);
        request.processing.vsr.scale = vsr.value("scale", 2.0);

        const auto hdr = processing.value("hdr", nlohmann::json::object());
        request.processing.hdr.enabled = hdr.value("enabled", false);
        request.processing.hdr.contrast = hdr.value("contrast", 100);
        request.processing.hdr.saturation = hdr.value("saturation", 100);
        request.processing.hdr.middle_gray = hdr.value("middleGray", 44);
        request.processing.hdr.max_luminance = hdr.value("maxLuminance", 1000);

        const auto output = json.value("output", nlohmann::json::object());
        request.output.container = output.value("container", "mp4");
        request.output.video_codec = output.value("videoCodec", request.processing.hdr.enabled ? "hevc" : "h264");
        request.output.audio_mode = output.value("audioMode", "copy");
        request.output.subtitle_mode = output.value("subtitleMode", "copy-compatible");

        const auto valid = validate_request(request);
        if (!valid.ok()) {
            return Result<TranscodeRequest>::Fail(valid.error());
        }
        return Result<TranscodeRequest>::Ok(request);
    } catch (const std::exception& ex) {
        return Result<TranscodeRequest>::Fail({"invalid_json", "Request JSON is invalid.", ex.what()});
    }
}

nlohmann::json error_to_json(const Error& error) {
    nlohmann::json json;
    json["error"] = {
        {"code", error.code},
        {"message", error.message},
        {"details", error.details}
    };
    return json;
}

nlohmann::json job_snapshot_to_json(const JobSnapshot& snapshot) {
    nlohmann::json json;
    json["id"] = snapshot.id;
    json["state"] = to_string(snapshot.state);
    json["stage"] = to_string(snapshot.progress.stage);
    json["progress"] = snapshot.progress.progress;
    json["framesDone"] = snapshot.progress.frames_done;
    json["framesTotal"] = snapshot.progress.frames_total;
    json["fps"] = snapshot.progress.fps;
    json["etaSeconds"] = snapshot.progress.eta_seconds;
    json["inputPath"] = snapshot.input_path;
    json["outputPath"] = snapshot.output_path;
    json["warnings"] = snapshot.warnings;
    if (!snapshot.error_code.empty()) {
        json["error"] = {
            {"code", snapshot.error_code},
            {"message", snapshot.error_message},
            {"details", snapshot.error_details}
        };
    }
    return json;
}

} // namespace vsr
```

- [ ] **Step 4: Create HTTP server files**

`src/api/http_server.h`:

```cpp
#pragma once

#include "jobs/job_runner.h"
#include "jobs/job_store.h"

#include <httplib.h>

#include <memory>
#include <string>

namespace vsr {

class HttpServer {
public:
    HttpServer(JobStore& store, JobRunner& runner);
    bool listen(const std::string& host, int port);
    void stop();

private:
    void bind_routes();

    JobStore& store_;
    JobRunner& runner_;
    httplib::Server server_;
};

} // namespace vsr
```

`src/api/http_server.cpp`:

```cpp
#include "api/http_server.h"

#include "api/json_dto.h"

#include <nlohmann/json.hpp>
#include <thread>

namespace vsr {

HttpServer::HttpServer(JobStore& store, JobRunner& runner) : store_(store), runner_(runner) {
    bind_routes();
}

void HttpServer::bind_routes() {
    server_.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(R"({"version":"0.1.0","ready":true})", "application/json");
    });

    server_.Post("/api/jobs", [this](const httplib::Request& request, httplib::Response& response) {
        const auto body = nlohmann::json::parse(request.body, nullptr, false);
        if (body.is_discarded()) {
            response.status = 400;
            response.set_content(error_to_json({"invalid_json", "Request JSON is invalid.", ""}).dump(), "application/json");
            return;
        }

        const auto parsed = parse_transcode_request(body);
        if (!parsed.ok()) {
            response.status = 400;
            response.set_content(error_to_json(parsed.error()).dump(), "application/json");
            return;
        }

        const auto created = store_.create(parsed.value());
        if (!created.ok()) {
            response.status = 400;
            response.set_content(error_to_json(created.error()).dump(), "application/json");
            return;
        }

        const std::string id = created.value();
        std::thread([this, id] { runner_.run_one(id); }).detach();
        response.status = 202;
        response.set_content(nlohmann::json{{"id", id}}.dump(), "application/json");
    });

    server_.Get(R"(/api/jobs/([0-9]+))", [this](const httplib::Request& request, httplib::Response& response) {
        const auto snapshot = store_.get(request.matches[1].str());
        if (!snapshot.ok()) {
            response.status = 404;
            response.set_content(error_to_json(snapshot.error()).dump(), "application/json");
            return;
        }
        response.set_content(job_snapshot_to_json(snapshot.value()).dump(), "application/json");
    });

    server_.Post(R"(/api/jobs/([0-9]+)/cancel)", [this](const httplib::Request& request, httplib::Response& response) {
        const auto result = store_.request_cancel(request.matches[1].str());
        if (!result.ok()) {
            response.status = 404;
            response.set_content(error_to_json(result.error()).dump(), "application/json");
            return;
        }
        response.set_content(R"({"accepted":true})", "application/json");
    });
}

bool HttpServer::listen(const std::string& host, int port) {
    return server_.listen(host, port);
}

void HttpServer::stop() {
    server_.stop();
}

} // namespace vsr
```

- [ ] **Step 5: Create `src/app/main.cpp`**

```cpp
#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"

#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    int port = 49321;
    if (argc >= 3 && std::string(argv[1]) == "--port") {
        port = std::stoi(argv[2]);
    }

    vsr::JobStore store;
    auto pipeline = std::make_unique<vsr::FakePipeline>(vsr::FakePipelineMode::succeeds);
    vsr::JobRunner runner(store, std::move(pipeline));
    vsr::HttpServer server(store, runner);

    std::cout << "vsr_backend listening on http://127.0.0.1:" << port << "\n";
    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "failed to listen on 127.0.0.1:" << port << "\n";
        return 1;
    }
    return 0;
}
```

- [ ] **Step 6: Run DTO tests and build executable**

```powershell
cmake --build build --target vsr_backend_tests vsr_backend
ctest --test-dir build -R JsonDto --output-on-failure
```

Expected: PASS for `JsonDto` tests and successful `vsr_backend.exe` build.

- [ ] **Step 7: Commit**

```powershell
git add src/api src/app/main.cpp tests/unit/json_dto_tests.cpp
git commit -m "feat: expose local job api"
```

---

### Task 5: Capability Detection

**Files:**
- Create: `src/platform/capabilities.h`
- Create: `src/platform/capabilities.cpp`
- Create: `tests/unit/capabilities_tests.cpp`
- Modify: `src/api/http_server.cpp`
- Modify: `src/api/http_server.h`

- [ ] **Step 1: Write failing capability tests**

Create `tests/unit/capabilities_tests.cpp`:

```cpp
#include "platform/capabilities.h"

#include <gtest/gtest.h>

using namespace vsr;

TEST(Capabilities, serializesUnavailableHardwareClearly) {
    CapabilitySnapshot snapshot;
    snapshot.d3d11_available = false;
    snapshot.rtx_sdk_found = false;
    snapshot.vsr_available = false;
    snapshot.truehdr_available = false;
    snapshot.nvenc_h264_available = false;
    snapshot.nvenc_hevc_main10_available = false;
    snapshot.messages.push_back("RTX SDK runtime DLLs were not found.");

    const auto json = capability_snapshot_to_json(snapshot);

    EXPECT_FALSE(json["d3d11Available"]);
    EXPECT_FALSE(json["rtxSdkFound"]);
    EXPECT_EQ(json["messages"][0], "RTX SDK runtime DLLs were not found.");
}
```

- [ ] **Step 2: Run tests and verify red**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R Capabilities --output-on-failure
```

Expected: FAIL because `platform/capabilities.h` does not exist.

- [ ] **Step 3: Create capability files**

`src/platform/capabilities.h`:

```cpp
#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace vsr {

struct CapabilitySnapshot {
    bool d3d11_available = false;
    bool rtx_sdk_found = false;
    bool vsr_available = false;
    bool truehdr_available = false;
    bool nvenc_h264_available = false;
    bool nvenc_hevc_main10_available = false;
    std::vector<std::string> messages;
};

CapabilitySnapshot detect_capabilities();
nlohmann::json capability_snapshot_to_json(const CapabilitySnapshot& snapshot);

} // namespace vsr
```

`src/platform/capabilities.cpp`:

```cpp
#include "platform/capabilities.h"

#include <filesystem>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#endif

namespace vsr {

CapabilitySnapshot detect_capabilities() {
    CapabilitySnapshot snapshot;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    const HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        nullptr,
        &context);
    snapshot.d3d11_available = SUCCEEDED(hr);
    if (!snapshot.d3d11_available) {
        snapshot.messages.push_back("D3D11 hardware device creation failed.");
    }
#else
    snapshot.messages.push_back("This backend supports Windows only.");
#endif

    snapshot.rtx_sdk_found =
        std::filesystem::exists("nvngx_vsr.dll") &&
        std::filesystem::exists("nvngx_truehdr.dll");
    if (!snapshot.rtx_sdk_found) {
        snapshot.messages.push_back("RTX SDK runtime DLLs were not found beside the backend executable.");
    }

    snapshot.vsr_available = snapshot.d3d11_available && snapshot.rtx_sdk_found;
    snapshot.truehdr_available = snapshot.d3d11_available && snapshot.rtx_sdk_found;
    snapshot.nvenc_h264_available = snapshot.d3d11_available;
    snapshot.nvenc_hevc_main10_available = snapshot.d3d11_available;
    return snapshot;
}

nlohmann::json capability_snapshot_to_json(const CapabilitySnapshot& snapshot) {
    return {
        {"d3d11Available", snapshot.d3d11_available},
        {"rtxSdkFound", snapshot.rtx_sdk_found},
        {"vsrAvailable", snapshot.vsr_available},
        {"truehdrAvailable", snapshot.truehdr_available},
        {"nvencH264Available", snapshot.nvenc_h264_available},
        {"nvencHevcMain10Available", snapshot.nvenc_hevc_main10_available},
        {"messages", snapshot.messages}
    };
}

} // namespace vsr
```

- [ ] **Step 4: Add `GET /api/capabilities`**

Modify `src/api/http_server.cpp` inside `bind_routes()`:

```cpp
server_.Get("/api/capabilities", [](const httplib::Request&, httplib::Response& response) {
    response.set_content(capability_snapshot_to_json(detect_capabilities()).dump(), "application/json");
});
```

Add include:

```cpp
#include "platform/capabilities.h"
```

- [ ] **Step 5: Run tests**

```powershell
cmake --build build --target vsr_backend_tests
ctest --test-dir build -R Capabilities --output-on-failure
```

Expected: PASS for capability serialization test.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/capabilities.* src/api/http_server.* tests/unit/capabilities_tests.cpp
git commit -m "feat: add backend capability endpoint"
```

---

### Task 6: RTX DX11 Processor Wrapper

**Files:**
- Create: `src/video/rtx/rtx_processor.h`
- Create: `src/video/rtx/rtx_dx11_processor.h`
- Create: `src/video/rtx/rtx_dx11_processor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create RTX abstraction**

`src/video/rtx/rtx_processor.h`:

```cpp
#pragma once

#include "core/result.h"
#include "jobs/job_types.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace vsr {

struct RtxDx11Frame {
    ID3D11Texture2D* input = nullptr;
    ID3D11Texture2D* output = nullptr;
    int input_width = 0;
    int input_height = 0;
    int output_width = 0;
    int output_height = 0;
};

class RtxProcessor {
public:
    virtual ~RtxProcessor() = default;
    virtual Result<void> initialize(ID3D11Device* device, const ProcessingSettings& settings) = 0;
    virtual Result<void> process(const RtxDx11Frame& frame, const ProcessingSettings& settings) = 0;
    virtual void shutdown() = 0;
};

} // namespace vsr
```

- [ ] **Step 2: Create DX11 wrapper header**

`src/video/rtx/rtx_dx11_processor.h`:

```cpp
#pragma once

#include "video/rtx/rtx_processor.h"

namespace vsr {

class RtxDx11Processor final : public RtxProcessor {
public:
    Result<void> initialize(ID3D11Device* device, const ProcessingSettings& settings) override;
    Result<void> process(const RtxDx11Frame& frame, const ProcessingSettings& settings) override;
    void shutdown() override;

private:
    bool initialized_ = false;
};

} // namespace vsr
```

- [ ] **Step 3: Create DX11 wrapper implementation**

`src/video/rtx/rtx_dx11_processor.cpp`:

```cpp
#include "video/rtx/rtx_dx11_processor.h"

#include "rtx_video_api.h"

namespace vsr {

namespace {

API_RECT make_rect(int width, int height) {
    API_RECT rect{};
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    return rect;
}

API_VSR_Setting make_vsr(const VsrSettings& settings) {
    API_VSR_Setting value{};
    value.QualityLevel = settings.quality;
    return value;
}

API_THDR_Setting make_hdr(const HdrSettings& settings) {
    API_THDR_Setting value{};
    value.Contrast = settings.contrast;
    value.Saturation = settings.saturation;
    value.MiddleGray = settings.middle_gray;
    value.MaxLuminance = settings.max_luminance;
    return value;
}

} // namespace

Result<void> RtxDx11Processor::initialize(ID3D11Device* device, const ProcessingSettings& settings) {
    if (device == nullptr) {
        return Result<void>::Fail({"d3d11_device_required", "A D3D11 device is required for RTX processing.", ""});
    }
    if (!settings.vsr.enabled && !settings.hdr.enabled) {
        return Result<void>::Fail({"processing_required", "Enable VSR, HDR, or both.", ""});
    }

    const API_BOOL created = rtx_video_api_dx11_create(
        device,
        settings.hdr.enabled ? API_BOOL_SUCCESS : API_BOOL_FAIL,
        settings.vsr.enabled ? API_BOOL_SUCCESS : API_BOOL_FAIL);

    if (created != API_BOOL_SUCCESS) {
        return Result<void>::Fail({"rtx_initialize_failed", "RTX Video SDK feature creation failed.", ""});
    }

    initialized_ = true;
    return Result<void>::Ok();
}

Result<void> RtxDx11Processor::process(const RtxDx11Frame& frame, const ProcessingSettings& settings) {
    if (!initialized_) {
        return Result<void>::Fail({"rtx_not_initialized", "RTX processor was not initialized.", ""});
    }
    if (frame.input == nullptr || frame.output == nullptr) {
        return Result<void>::Fail({"rtx_texture_required", "Input and output D3D11 textures are required.", ""});
    }

    auto input_rect = make_rect(frame.input_width, frame.input_height);
    auto output_rect = make_rect(frame.output_width, frame.output_height);
    auto vsr = make_vsr(settings.vsr);
    auto hdr = make_hdr(settings.hdr);

    const API_BOOL result = rtx_video_api_dx11_evaluate(
        frame.input,
        frame.output,
        input_rect,
        output_rect,
        settings.vsr.enabled ? &vsr : nullptr,
        settings.hdr.enabled ? &hdr : nullptr);

    if (result != API_BOOL_SUCCESS) {
        return Result<void>::Fail({"rtx_evaluate_failed", "RTX Video SDK evaluation failed.", ""});
    }

    return Result<void>::Ok();
}

void RtxDx11Processor::shutdown() {
    if (initialized_) {
        rtx_video_api_dx11_shutdown();
        initialized_ = false;
    }
}

} // namespace vsr
```

- [ ] **Step 4: Build with SDK enabled**

```powershell
cmake -S . -B build-rtx -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=ON
cmake --build build-rtx --target vsr_backend --config Release
```

Expected: `vsr_backend.exe` builds and `nvngx_vsr.dll` plus `nvngx_truehdr.dll` are copied beside it. If Windows SDK, DirectX headers, or NVIDIA SDK libs are missing, the command fails with the missing component named by MSBuild or CMake.

- [ ] **Step 5: Commit**

```powershell
git add src/video/rtx CMakeLists.txt
git commit -m "feat: wrap rtx video dx11 processor"
```

---

### Task 7: FFmpeg Probe And Real Pipeline Shell

**Files:**
- Create: `src/video/ffmpeg/ffmpeg_probe.h`
- Create: `src/video/ffmpeg/ffmpeg_probe.cpp`
- Create: `src/video/ffmpeg/ffmpeg_transcode_pipeline.h`
- Create: `src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`
- Modify: `src/app/main.cpp`

- [ ] **Step 1: Create FFmpeg probe header**

`src/video/ffmpeg/ffmpeg_probe.h`:

```cpp
#pragma once

#include "core/result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vsr {

struct MediaProbe {
    std::string path;
    int video_stream_index = -1;
    int width = 0;
    int height = 0;
    std::int64_t frame_count = 0;
    double duration_seconds = 0.0;
    std::vector<std::string> warnings;
};

Result<MediaProbe> probe_media(const std::string& path);

} // namespace vsr
```

- [ ] **Step 2: Create FFmpeg probe implementation**

`src/video/ffmpeg/ffmpeg_probe.cpp`:

```cpp
#include "video/ffmpeg/ffmpeg_probe.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace vsr {

Result<MediaProbe> probe_media(const std::string& path) {
    AVFormatContext* format = nullptr;
    if (avformat_open_input(&format, path.c_str(), nullptr, nullptr) < 0) {
        return Result<MediaProbe>::Fail({"input_open_failed", "FFmpeg could not open the input file.", path});
    }

    if (avformat_find_stream_info(format, nullptr) < 0) {
        avformat_close_input(&format);
        return Result<MediaProbe>::Fail({"stream_info_failed", "FFmpeg could not read stream information.", path});
    }

    MediaProbe probe;
    probe.path = path;
    probe.duration_seconds = format->duration > 0 ? static_cast<double>(format->duration) / AV_TIME_BASE : 0.0;

    for (unsigned int i = 0; i < format->nb_streams; ++i) {
        const AVStream* stream = format->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && probe.video_stream_index < 0) {
            probe.video_stream_index = static_cast<int>(i);
            probe.width = stream->codecpar->width;
            probe.height = stream->codecpar->height;
            probe.frame_count = stream->nb_frames;
        }
    }

    avformat_close_input(&format);

    if (probe.video_stream_index < 0) {
        return Result<MediaProbe>::Fail({"video_stream_missing", "Input file does not contain a video stream.", path});
    }

    return Result<MediaProbe>::Ok(probe);
}

} // namespace vsr
```

- [ ] **Step 3: Create transcode pipeline header**

`src/video/ffmpeg/ffmpeg_transcode_pipeline.h`:

```cpp
#pragma once

#include "video/rtx/rtx_processor.h"
#include "video/video_pipeline.h"

#include <memory>

namespace vsr {

class FfmpegTranscodePipeline final : public VideoPipeline {
public:
    explicit FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx);
    Result<void> run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) override;

private:
    std::unique_ptr<RtxProcessor> rtx_;
};

} // namespace vsr
```

- [ ] **Step 4: Create transcode pipeline implementation**

`src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`:

```cpp
#include "video/ffmpeg/ffmpeg_transcode_pipeline.h"

#include "video/ffmpeg/ffmpeg_probe.h"

#include <filesystem>

namespace vsr {

FfmpegTranscodePipeline::FfmpegTranscodePipeline(std::unique_ptr<RtxProcessor> rtx) : rtx_(std::move(rtx)) {}

Result<void> FfmpegTranscodePipeline::run(const TranscodeRequest& request, CancellationToken& cancellation, ProgressCallback progress) {
    if (!std::filesystem::exists(request.input_path)) {
        return Result<void>::Fail({"input_not_found", "Input file does not exist.", request.input_path});
    }

    JobProgress validating;
    validating.stage = JobStage::validating;
    validating.progress = 0.01;
    progress(validating);

    const auto probe = probe_media(request.input_path);
    if (!probe.ok()) {
        return Result<void>::Fail(probe.error());
    }

    JobProgress probed;
    probed.stage = JobStage::probing;
    probed.progress = 0.05;
    probed.frames_total = probe.value().frame_count;
    progress(probed);

    if (cancellation.requested.load()) {
        return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
    }

    return Result<void>::Fail({
        "hardware_pipeline_not_connected",
        "FFmpeg probing is connected; the D3D11 decode/process/encode loop is not connected in this task.",
        "Continue with Task 8 to connect the hardware frame loop."
    });
}

} // namespace vsr
```

- [ ] **Step 5: Build with FFmpeg enabled**

```powershell
cmake -S . -B build-ffmpeg -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=OFF -DFFMPEG_ROOT=C:\ffmpeg
cmake --build build-ffmpeg --target vsr_backend --config Release
```

Expected: build succeeds when `FFMPEG_ROOT` points to an FFmpeg developer package containing headers and import libraries. If `FFMPEG_ROOT` is wrong, CMake fails with `Could NOT find FFmpeg`.

- [ ] **Step 6: Commit**

```powershell
git add src/video/ffmpeg src/app/main.cpp CMakeLists.txt
git commit -m "feat: add ffmpeg pipeline shell"
```

---

### Task 8: Connect D3D11 Decode, RTX Process, And NVENC Encode

**Files:**
- Modify: `src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`
- Modify: `src/video/ffmpeg/ffmpeg_transcode_pipeline.h`
- Modify: `src/platform/capabilities.cpp`
- Create: `tests/scripts/hardware_smoke.ps1`

- [ ] **Step 1: Add a manual smoke script**

Create `tests/scripts/hardware_smoke.ps1`:

```powershell
param(
  [Parameter(Mandatory = $true)][string]$BackendExe,
  [Parameter(Mandatory = $true)][string]$InputPath,
  [Parameter(Mandatory = $true)][string]$OutputPath,
  [int]$Port = 49321
)

$process = Start-Process -FilePath $BackendExe -ArgumentList "--port", $Port -PassThru -WindowStyle Hidden
try {
  Start-Sleep -Seconds 1
  $health = Invoke-RestMethod "http://127.0.0.1:$Port/api/health"
  if (-not $health.ready) { throw "Backend health check failed." }

  $body = @{
    inputPath = $InputPath
    outputPath = $OutputPath
    processing = @{
      vsr = @{ enabled = $true; quality = 3; scale = 2.0 }
      hdr = @{ enabled = $true; contrast = 100; saturation = 100; middleGray = 44; maxLuminance = 1000 }
    }
    output = @{ container = "mp4"; videoCodec = "hevc"; audioMode = "copy"; subtitleMode = "copy-compatible" }
  } | ConvertTo-Json -Depth 8

  $created = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$Port/api/jobs" -Body $body -ContentType "application/json"
  do {
    Start-Sleep -Milliseconds 500
    $job = Invoke-RestMethod "http://127.0.0.1:$Port/api/jobs/$($created.id)"
    Write-Host "$($job.state) $($job.stage) $($job.progress)"
  } while ($job.state -eq "queued" -or $job.state -eq "running" -or $job.state -eq "canceling")

  if ($job.state -ne "succeeded") {
    throw "Job ended as $($job.state): $($job.error.code) $($job.error.message) $($job.error.details)"
  }
  if (-not (Test-Path $OutputPath)) {
    throw "Output file was not created."
  }
}
finally {
  Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}
```

- [ ] **Step 2: Replace the Task 7 pipeline body with the hardware frame loop**

Implementation requirements for `src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`:

```cpp
// The implementation must keep these exact checkpoints:
// 1. avformat_open_input
// 2. avformat_find_stream_info
// 3. avcodec_find_decoder for the primary video stream
// 4. create one ID3D11Device
// 5. create AVHWDeviceContext of type AV_HWDEVICE_TYPE_D3D11VA
// 6. open decoder with hw_device_ctx
// 7. create output AVFormatContext for MP4
// 8. create NVENC encoder: h264_nvenc for SDR VSR-only, hevc_nvenc for HDR
// 9. set HEVC Main10 and HDR color metadata when hdr.enabled is true
// 10. initialize RtxDx11Processor with the same D3D11 device
// 11. for each decoded video frame:
//     a. stop if cancellation.requested is true
//     b. extract ID3D11Texture2D from the D3D11VA AVFrame
//     c. allocate or reuse output ID3D11Texture2D matching output dimensions and encoder format
//     d. call rtx_->process(frame, request.processing)
//     e. wrap output texture in an encoder AVFrame
//     f. send frame to NVENC and mux packets
//     g. emit JobProgress with stage processing_rtx or encoding
// 12. flush decoder
// 13. flush encoder
// 14. write trailer
// 15. release RTX processor
```

Use this error mapping inside the implementation:

```cpp
static Error ffmpeg_error(const char* code, const char* message, int ffmpeg_code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(ffmpeg_code, buffer, sizeof(buffer));
    return {code, message, buffer};
}
```

Use this progress calculation:

```cpp
static double progress_from_frames(std::int64_t frames_done, std::int64_t frames_total) {
    if (frames_total <= 0) {
        return 0.10;
    }
    const double value = static_cast<double>(frames_done) / static_cast<double>(frames_total);
    return value < 0.10 ? 0.10 : (value > 0.98 ? 0.98 : value);
}
```

The first connected implementation may reject non-D3D11 hardware frames with:

```cpp
return Result<void>::Fail({
    "d3d11_frame_required",
    "The hardware pipeline requires FFmpeg D3D11VA frames.",
    "Use a Windows FFmpeg build with D3D11VA and NVENC support."
});
```

- [ ] **Step 3: Build hardware target**

```powershell
cmake -S . -B build-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT=C:\ffmpeg
cmake --build build-hw --target vsr_backend --config Release
```

Expected: Release build succeeds and copies RTX SDK runtime DLLs beside `vsr_backend.exe`.

- [ ] **Step 4: Run manual hardware smoke**

```powershell
tests/scripts/hardware_smoke.ps1 `
  -BackendExe .\build-hw\Release\vsr_backend.exe `
  -InputPath C:\Videos\short-sdr.mp4 `
  -OutputPath C:\Videos\short-sdr.rtx.mp4
```

Expected: the script prints progress states and exits successfully after creating the output MP4. If the machine lacks an RTX GPU, NVIDIA driver support, FFmpeg D3D11VA, or NVENC, the script exits with the structured backend error.

- [ ] **Step 5: Commit**

```powershell
git add src/video/ffmpeg/ffmpeg_transcode_pipeline.* src/platform/capabilities.cpp tests/scripts/hardware_smoke.ps1
git commit -m "feat: connect hardware transcode pipeline"
```

---

### Task 9: Logging, Cancellation Propagation, And Frontend Integration Docs

**Files:**
- Create: `src/platform/logging.h`
- Create: `src/platform/logging.cpp`
- Modify: `src/jobs/job_runner.cpp`
- Modify: `src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`
- Create: `docs/frontend-integration.md`

- [ ] **Step 1: Create logger**

`src/platform/logging.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>

namespace vsr {

void initialize_logging(const std::filesystem::path& directory);
void log_info(const std::string& message);
void log_error(const std::string& message);
std::filesystem::path current_log_path();

} // namespace vsr
```

`src/platform/logging.cpp`:

```cpp
#include "platform/logging.h"

#include <chrono>
#include <fstream>
#include <mutex>

namespace vsr {

namespace {
std::mutex g_log_mutex;
std::filesystem::path g_log_path = "vsr_backend.log";

void write_line(const std::string& level, const std::string& message) {
    std::lock_guard lock(g_log_mutex);
    std::ofstream file(g_log_path, std::ios::app);
    file << level << " " << message << "\n";
}
}

void initialize_logging(const std::filesystem::path& directory) {
    std::lock_guard lock(g_log_mutex);
    std::filesystem::create_directories(directory);
    g_log_path = directory / "vsr_backend.log";
}

void log_info(const std::string& message) {
    write_line("INFO", message);
}

void log_error(const std::string& message) {
    write_line("ERROR", message);
}

std::filesystem::path current_log_path() {
    std::lock_guard lock(g_log_mutex);
    return g_log_path;
}

} // namespace vsr
```

- [ ] **Step 2: Wire cancellation into runner**

Modify `JobRunner` to keep a cancellation token per running job instead of constructing a stack token that API cancellation cannot reach. The implementation must:

```cpp
// Add to JobRunner private members:
std::mutex cancellation_mutex_;
std::unordered_map<std::string, std::shared_ptr<CancellationToken>> cancellations_;

// Add public method:
void request_cancel(const std::string& id);

// In run_one:
auto cancellation = std::make_shared<CancellationToken>();
{
    std::lock_guard lock(cancellation_mutex_);
    cancellations_[id] = cancellation;
}
// Pass *cancellation to pipeline_->run(...)
// Erase cancellations_[id] before returning.

// In request_cancel:
store_.request_cancel(id);
std::lock_guard lock(cancellation_mutex_);
if (auto it = cancellations_.find(id); it != cancellations_.end()) {
    it->second->requested.store(true);
}
```

Update the HTTP cancel route to call `runner_.request_cancel(id)`.

- [ ] **Step 3: Create frontend integration document**

Create `docs/frontend-integration.md` with these sections:

```markdown
# Frontend Integration Guide

## Backend Process

Start `vsr_backend.exe --port 49321` as a local sidecar process. The backend listens only on `127.0.0.1`.

## Health

`GET http://127.0.0.1:49321/api/health`

Response:

```json
{ "version": "0.1.0", "ready": true }
```

## Capabilities

`GET /api/capabilities`

Use this endpoint before enabling the main convert button. Show missing requirements from `messages`.

## Create Job

`POST /api/jobs`

Request:

```json
{
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "processing": {
    "vsr": { "enabled": true, "quality": 3, "scale": 2.0 },
    "hdr": { "enabled": true, "contrast": 100, "saturation": 100, "middleGray": 44, "maxLuminance": 1000 }
  },
  "output": { "container": "mp4", "videoCodec": "hevc", "audioMode": "copy", "subtitleMode": "copy-compatible" }
}
```

Response:

```json
{ "id": "00000000000000000000000001" }
```

## Poll Job

Poll `GET /api/jobs/{id}` every 500 ms while state is `queued`, `running`, or `canceling`.

Terminal states are `succeeded`, `failed`, and `canceled`.

## Cancel Job

`POST /api/jobs/{id}/cancel`

Response:

```json
{ "accepted": true }
```

## UI Parameter Ranges

- VSR quality: integer 1 to 4, default 3.
- VSR scale: 1.0 to 4.0, default 2.0.
- HDR contrast: integer, default 100.
- HDR saturation: integer, default 100.
- HDR middle gray: integer, default 44.
- HDR max luminance: integer nits, default 1000.
- HDR output codec: `hevc`.
- VSR-only default codec: `h264`.

## Recommended UI States

- Disable Convert until health is ready and capabilities are loaded.
- Show capability warnings above settings.
- During a job, show stage, percent, frames, fps, and ETA.
- Keep Cancel visible while state is `queued` or `running`.
- On failure, show `error.message` and put `error.details` behind an expandable details row.
```

- [ ] **Step 4: Run unit tests and build**

```powershell
cmake --build build --target vsr_backend_tests vsr_backend
ctest --test-dir build --output-on-failure
```

Expected: all unit tests pass in the non-hardware build.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/logging.* src/jobs/job_runner.* src/api/http_server.cpp src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp docs/frontend-integration.md
git commit -m "feat: add cancellation logging and frontend docs"
```

---

### Task 10: Final Verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Create README**

Create `README.md`:

```markdown
# RTX Video Backend

Windows-only local backend for complete-video RTX VSR and RTX Video HDR transcoding.

## Build Unit-Test Backend

```powershell
cmake -S . -B build -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build --target vsr_backend_tests vsr_backend
ctest --test-dir build --output-on-failure
```

## Build Hardware Backend

Install a Windows FFmpeg developer package with D3D11VA and NVENC support, then set `FFMPEG_ROOT`.

```powershell
cmake -S . -B build-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT=C:\ffmpeg
cmake --build build-hw --target vsr_backend --config Release
```

## Run

```powershell
.\build-hw\Release\vsr_backend.exe --port 49321
```

Open `docs/frontend-integration.md` for API details.
```

- [ ] **Step 2: Run non-hardware verification**

```powershell
cmake -S . -B build -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build --target vsr_backend_tests vsr_backend
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds, build succeeds, all unit tests pass.

- [ ] **Step 3: Run hardware verification on an RTX Windows machine**

```powershell
cmake -S . -B build-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT=C:\ffmpeg
cmake --build build-hw --target vsr_backend --config Release
tests/scripts/hardware_smoke.ps1 `
  -BackendExe .\build-hw\Release\vsr_backend.exe `
  -InputPath C:\Videos\short-sdr.mp4 `
  -OutputPath C:\Videos\short-sdr.rtx.mp4
```

Expected: hardware build succeeds and smoke script creates an MP4 output file.

- [ ] **Step 4: Check git status**

```powershell
git status --short
```

Expected: only intentionally untracked local SDK files remain, or a clean tree if the SDK files are ignored or committed separately by user decision.

- [ ] **Step 5: Commit**

```powershell
git add README.md
git commit -m "docs: add backend build instructions"
```

---

## Self-Review

- Spec coverage: covered local API, job model, MP4 output, RTX VSR/TrueHDR processing, FFmpeg, D3D11, capability detection, cancellation, logging, frontend documentation, and verification.
- Placeholder scan: checked for unresolved marker phrases and vague implementation instructions; none remain.
- Type consistency: `TranscodeRequest`, `ProcessingSettings`, `JobSnapshot`, `JobRunner`, `VideoPipeline`, `RtxProcessor`, `CapabilitySnapshot`, and endpoint JSON names are consistent across tasks.
