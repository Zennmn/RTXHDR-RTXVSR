# Tauri Frontend Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the existing React/Vite UI into a Tauri Windows app that launches the C++ backend sidecar, selects real files/folders, probes media, submits jobs, polls progress, cancels jobs, and shows backend errors.

**Architecture:** Keep the C++ backend as the local HTTP source of truth. Add only the backend endpoints and CORS behavior needed by the desktop/webview frontend. Split frontend logic into typed API, request building, Tauri bridge, hooks, and focused UI components.

**Tech Stack:** C++20, cpp-httplib, nlohmann/json, GoogleTest, React 19, TypeScript, Vite, Tailwind CSS v4, Vitest, Tauri v2, Tauri dialog plugin, Tauri shell plugin.

---

## File Structure

Backend files:

- Modify: `backend/src/api/http_server.cpp` to add CORS preflight/default headers and `POST /api/media/probe`.
- Modify: `backend/src/api/json_dto.h` to add media probe DTO structs and functions.
- Modify: `backend/src/api/json_dto.cpp` to parse probe requests and serialize probe responses.
- Modify: `backend/src/video/ffmpeg/ffmpeg_probe.h` to include codec metadata.
- Modify: `backend/src/video/ffmpeg/ffmpeg_probe.cpp` to fill codec metadata when FFmpeg is enabled.
- Create: `backend/src/video/media_probe_service.h` for UI-facing media metadata.
- Create: `backend/src/video/media_probe_service.cpp` for file-size/name fallback and optional FFmpeg detail probing.
- Modify: `backend/CMakeLists.txt` to compile `media_probe_service.cpp` and new tests.
- Create: `backend/tests/unit/media_probe_tests.cpp` for DTO/service behavior.
- Create: `backend/tests/unit/http_server_tests.cpp` for CORS and probe route behavior.

Frontend files:

- Modify: `frontend/rtx-video-converter/package.json` to remove AI Studio dependencies, add Tauri/Vitest scripts, and rename the app.
- Modify: `frontend/rtx-video-converter/index.html` to set the product title.
- Modify: `frontend/rtx-video-converter/.env.example` to document `VITE_BACKEND_BASE_URL`.
- Delete: `frontend/rtx-video-converter/metadata.json`.
- Modify: `frontend/rtx-video-converter/README.md` to document dev and Tauri workflows.
- Create: `frontend/rtx-video-converter/src/api/backendClient.ts`.
- Create: `frontend/rtx-video-converter/src/api/backendClient.test.ts`.
- Create: `frontend/rtx-video-converter/src/api/types.ts`.
- Create: `frontend/rtx-video-converter/src/lib/jobRequest.ts`.
- Create: `frontend/rtx-video-converter/src/lib/jobRequest.test.ts`.
- Create: `frontend/rtx-video-converter/src/lib/tauriBridge.ts`.
- Create: `frontend/rtx-video-converter/src/hooks/useBackendStatus.ts`.
- Create: `frontend/rtx-video-converter/src/hooks/useTranscodeJob.ts`.
- Create: `frontend/rtx-video-converter/src/components/CapabilityBanner.tsx`.
- Create: `frontend/rtx-video-converter/src/components/ErrorDetails.tsx`.
- Create: `frontend/rtx-video-converter/src/components/InputPanel.tsx`.
- Create: `frontend/rtx-video-converter/src/components/SettingsPanel.tsx`.
- Create: `frontend/rtx-video-converter/src/components/StatusFooter.tsx`.
- Modify: `frontend/rtx-video-converter/src/App.tsx` to wire hooks and components.
- Modify: `frontend/rtx-video-converter/src/types.ts` to replace mock-only types with UI types.
- Create: `frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1`.
- Create: `frontend/rtx-video-converter/src-tauri/Cargo.toml`.
- Create: `frontend/rtx-video-converter/src-tauri/build.rs`.
- Create: `frontend/rtx-video-converter/src-tauri/src/main.rs`.
- Create: `frontend/rtx-video-converter/src-tauri/src/lib.rs`.
- Create: `frontend/rtx-video-converter/src-tauri/tauri.conf.json`.
- Create: `frontend/rtx-video-converter/src-tauri/capabilities/default.json`.
- Create: `frontend/rtx-video-converter/src-tauri/binaries/.gitkeep`.

---

### Task 1: Frontend Project Identity And Dependencies

**Files:**

- Modify: `frontend/rtx-video-converter/package.json`
- Modify: `frontend/rtx-video-converter/index.html`
- Modify: `frontend/rtx-video-converter/.env.example`
- Modify: `frontend/rtx-video-converter/README.md`
- Delete: `frontend/rtx-video-converter/metadata.json`

- [ ] **Step 1: Replace `package.json` with app-specific scripts and dependencies**

Use this complete file:

```json
{
  "name": "rtx-video-converter",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite --port=3000 --host=127.0.0.1",
    "build": "tsc --noEmit && vite build",
    "preview": "vite preview",
    "lint": "tsc --noEmit",
    "test": "vitest run",
    "test:watch": "vitest",
    "sidecar:prepare": "powershell -ExecutionPolicy Bypass -File scripts/prepare-tauri-sidecar.ps1",
    "tauri": "tauri",
    "tauri:dev": "tauri dev",
    "tauri:build": "tauri build"
  },
  "dependencies": {
    "@tailwindcss/vite": "^4.1.14",
    "@tauri-apps/api": "^2.11.0",
    "@tauri-apps/plugin-dialog": "^2.7.1",
    "@tauri-apps/plugin-shell": "^2.3.5",
    "@vitejs/plugin-react": "^5.0.4",
    "lucide-react": "^0.546.0",
    "react": "^19.0.1",
    "react-dom": "^19.0.1",
    "tailwindcss": "^4.1.14",
    "vite": "^6.2.3"
  },
  "devDependencies": {
    "@tauri-apps/cli": "^2.11.2",
    "@types/node": "^22.14.0",
    "typescript": "~5.8.2",
    "vitest": "^4.1.8"
  }
}
```

- [ ] **Step 2: Replace `index.html` product title**

Use this complete file:

```html
<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>RTX Video Converter</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

- [ ] **Step 3: Replace `.env.example`**

Use this complete file:

```dotenv
# Optional override for development. Defaults to http://127.0.0.1:49321.
VITE_BACKEND_BASE_URL=http://127.0.0.1:49321
```

- [ ] **Step 4: Replace `README.md`**

Use this complete file:

```markdown
# RTX Video Converter Frontend

React/Vite/Tauri frontend for the RTXHDR-RTXVSR Windows app.

## Browser Development

Start the backend manually:

```powershell
.\build\backend\Debug\vsr_backend.exe --port 49321
```

Then run the frontend:

```powershell
npm install
npm run dev
```

Browser mode can call the backend but cannot open native Windows file/folder dialogs.

## Tauri Development

Prepare a backend sidecar binary after building `vsr_backend.exe`:

```powershell
npm run sidecar:prepare -- -BackendExe ..\..\build\backend\Debug\vsr_backend.exe
```

Then run:

```powershell
npm run tauri:dev
```

Tauri requires Rust and Cargo in `PATH`.
```

- [ ] **Step 5: Delete `metadata.json`**

Run:

```powershell
Remove-Item -LiteralPath frontend\rtx-video-converter\metadata.json
```

Expected: `metadata.json` is removed.

- [ ] **Step 6: Install dependencies and generate lockfile**

Run:

```powershell
cd frontend\rtx-video-converter
npm install
```

Expected: `package-lock.json` is created and npm exits with code 0.

- [ ] **Step 7: Run frontend typecheck**

Run:

```powershell
cd frontend\rtx-video-converter
npm run lint
```

Expected: TypeScript still reports existing source errors only if the current UI has invalid types. Fix any dependency-related errors before continuing.

- [ ] **Step 8: Commit**

```powershell
git add frontend/rtx-video-converter/package.json frontend/rtx-video-converter/package-lock.json frontend/rtx-video-converter/index.html frontend/rtx-video-converter/.env.example frontend/rtx-video-converter/README.md frontend/rtx-video-converter/metadata.json
git commit -m "chore: prepare frontend app dependencies"
```

---

### Task 2: Backend Media Probe Tests

**Files:**

- Create: `backend/tests/unit/media_probe_tests.cpp`
- Create: `backend/tests/unit/http_server_tests.cpp`
- Modify: `backend/CMakeLists.txt`

- [ ] **Step 1: Add media probe DTO and service tests**

Create `backend/tests/unit/media_probe_tests.cpp`:

```cpp
#include "api/json_dto.h"
#include "video/media_probe_service.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace vsr;

TEST(MediaProbeDto, rejectsMissingInputPath) {
    const auto parsed = parse_media_probe_request(nlohmann::json::object());

    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code, "input_path_required");
}

TEST(MediaProbeDto, parsesInputPath) {
    const auto parsed = parse_media_probe_request({{"inputPath", "C:\\Videos\\input.mp4"}});

    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_EQ(parsed.value().input_path, "C:\\Videos\\input.mp4");
}

TEST(MediaProbeDto, serializesUiMetadata) {
    MediaProbeSummary summary;
    summary.path = "C:\\Videos\\input.mp4";
    summary.name = "input.mp4";
    summary.size_bytes = 1234;
    summary.resolution = "1920x1080";
    summary.duration = "00:01:05";
    summary.codec = "h264 8-bit";
    summary.warnings = {"metadata warning"};

    const auto json = media_probe_summary_to_json(summary);

    EXPECT_EQ(json["path"], summary.path);
    EXPECT_EQ(json["name"], summary.name);
    EXPECT_EQ(json["sizeBytes"], 1234);
    EXPECT_EQ(json["resolution"], summary.resolution);
    EXPECT_EQ(json["duration"], summary.duration);
    EXPECT_EQ(json["codec"], summary.codec);
    EXPECT_EQ(json["warnings"][0], "metadata warning");
}

TEST(MediaProbeService, returnsFallbackFileMetadataWhenDetailedProbeIsUnavailable) {
    const auto path = std::filesystem::temp_directory_path() / "vsr-media-probe-fallback.bin";
    {
        std::ofstream output(path, std::ios::binary);
        output << "abcd";
    }

    const auto result = probe_media_for_ui(path.string());

    std::filesystem::remove(path);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().name, path.filename().string());
    EXPECT_EQ(result.value().size_bytes, 4);
#if !defined(VSR_ENABLE_FFMPEG)
    EXPECT_EQ(result.value().resolution, "");
    EXPECT_EQ(result.value().duration, "");
    EXPECT_EQ(result.value().codec, "");
    ASSERT_FALSE(result.value().warnings.empty());
#endif
}
```

- [ ] **Step 2: Add HTTP server tests**

Create `backend/tests/unit/http_server_tests.cpp`:

```cpp
#include "api/http_server.h"
#include "jobs/job_runner.h"
#include "jobs/job_store.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

using namespace vsr;

namespace {

class TestHttpServer {
public:
    TestHttpServer()
        : port_(next_port_.fetch_add(1)),
          runner_(store_, std::make_unique<FakePipeline>(FakePipelineMode::succeeds)),
          server_(store_, runner_) {}

    ~TestHttpServer() {
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    int port() const { return port_; }

    void start() {
        thread_ = std::thread([this] {
            server_.listen("127.0.0.1", port_);
        });

        httplib::Client client("127.0.0.1", port_);
        for (int i = 0; i < 40; ++i) {
            if (const auto response = client.Get("/api/health"); response && response->status == 200) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        FAIL() << "test server did not become ready";
    }

private:
    static std::atomic_int next_port_;
    int port_;
    JobStore store_;
    JobRunner runner_;
    HttpServer server_;
    std::thread thread_;
};

std::atomic_int TestHttpServer::next_port_{49322};

} // namespace

TEST(HttpServer, respondsToCorsPreflight) {
    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Options("/api/jobs");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 204);
    EXPECT_EQ(response->headers.at("Access-Control-Allow-Origin"), "*");
}

TEST(HttpServer, rejectsInvalidProbeJson) {
    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/media/probe", "{", "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_NE(response->body.find("invalid_json"), std::string::npos);
}

TEST(HttpServer, probesExistingFileWithFallbackMetadata) {
    const auto path = std::filesystem::temp_directory_path() / "vsr-http-probe.bin";
    {
        std::ofstream output(path, std::ios::binary);
        output << "abcdef";
    }

    TestHttpServer server;
    server.start();
    httplib::Client client("127.0.0.1", server.port());
    const std::string body = std::string("{\"inputPath\":\"") + path.string() + "\"}";

    const auto response = client.Post("/api/media/probe", body, "application/json");

    std::filesystem::remove(path);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    EXPECT_NE(response->body.find("\"name\":\"vsr-http-probe.bin\""), std::string::npos);
    EXPECT_NE(response->body.find("\"sizeBytes\":6"), std::string::npos);
}
```

- [ ] **Step 3: Add new test files to CMake**

Modify `_vsr_test_sources` in `backend/CMakeLists.txt`:

```cmake
  set(_vsr_test_sources
    tests/unit/bootstrap_tests.cpp
    tests/unit/job_request_tests.cpp
    tests/unit/job_store_tests.cpp
    tests/unit/job_runner_tests.cpp
    tests/unit/json_dto_tests.cpp
    tests/unit/capabilities_tests.cpp
    tests/unit/logging_tests.cpp
    tests/unit/ffmpeg_transcode_pipeline_tests.cpp
    tests/unit/media_probe_tests.cpp
    tests/unit/http_server_tests.cpp
  )
```

- [ ] **Step 4: Run tests and verify failure**

Run:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests
```

Expected: build fails because `video/media_probe_service.h`, `parse_media_probe_request`, and `media_probe_summary_to_json` do not exist yet.

- [ ] **Step 5: Commit failing tests**

```powershell
git add backend/tests/unit/media_probe_tests.cpp backend/tests/unit/http_server_tests.cpp backend/CMakeLists.txt
git commit -m "test: define media probe backend behavior"
```

---

### Task 3: Backend Media Probe And CORS Implementation

**Files:**

- Modify: `backend/src/api/http_server.cpp`
- Modify: `backend/src/api/json_dto.h`
- Modify: `backend/src/api/json_dto.cpp`
- Modify: `backend/src/video/ffmpeg/ffmpeg_probe.h`
- Modify: `backend/src/video/ffmpeg/ffmpeg_probe.cpp`
- Create: `backend/src/video/media_probe_service.h`
- Create: `backend/src/video/media_probe_service.cpp`
- Modify: `backend/CMakeLists.txt`

- [ ] **Step 1: Add media probe DTO declarations**

In `backend/src/api/json_dto.h`, add these declarations after includes and before existing functions:

```cpp
#include <cstdint>
#include <string>
#include <vector>

struct MediaProbeRequest {
    std::string input_path;
};

struct MediaProbeSummary {
    std::string path;
    std::string name;
    std::uint64_t size_bytes = 0;
    std::string resolution;
    std::string duration;
    std::string codec;
    std::vector<std::string> warnings;
};

Result<MediaProbeRequest> parse_media_probe_request(const nlohmann::json& json);
nlohmann::json media_probe_summary_to_json(const MediaProbeSummary& summary);
```

- [ ] **Step 2: Implement media probe DTO functions**

In `backend/src/api/json_dto.cpp`, add:

```cpp
Result<MediaProbeRequest> parse_media_probe_request(const nlohmann::json& json) {
    try {
        MediaProbeRequest request;
        request.input_path = json.value("inputPath", "");
        if (request.input_path.empty()) {
            return Result<MediaProbeRequest>::Fail({"input_path_required", "Input path is required.", ""});
        }
        return Result<MediaProbeRequest>::Ok(std::move(request));
    } catch (const std::exception& ex) {
        return Result<MediaProbeRequest>::Fail({"invalid_json", "Request JSON is invalid.", ex.what()});
    }
}

nlohmann::json media_probe_summary_to_json(const MediaProbeSummary& summary) {
    return {
        {"path", summary.path},
        {"name", summary.name},
        {"sizeBytes", summary.size_bytes},
        {"resolution", summary.resolution},
        {"duration", summary.duration},
        {"codec", summary.codec},
        {"warnings", summary.warnings},
    };
}
```

- [ ] **Step 3: Extend FFmpeg probe metadata**

Modify `backend/src/video/ffmpeg/ffmpeg_probe.h`:

```cpp
struct MediaProbe {
    std::string path;
    int video_stream_index = -1;
    int width = 0;
    int height = 0;
    std::int64_t frame_count = 0;
    double duration_seconds = 0.0;
    std::string codec_name;
    int bit_depth = 0;
    std::vector<std::string> warnings;
};
```

Modify `backend/src/video/ffmpeg/ffmpeg_probe.cpp` includes:

```cpp
#include <libavcodec/codec_id.h>
#include <libavutil/pixdesc.h>
```

Then fill metadata after width/height:

```cpp
    probe.codec_name = avcodec_get_name(stream->codecpar->codec_id);
    const AVPixFmtDescriptor* descriptor =
        av_pix_fmt_desc_get(static_cast<AVPixelFormat>(stream->codecpar->format));
    if (descriptor != nullptr) {
        probe.bit_depth = descriptor->comp[0].depth;
    }
```

- [ ] **Step 4: Add UI-facing media probe service**

Create `backend/src/video/media_probe_service.h`:

```cpp
#pragma once

#include "api/json_dto.h"
#include "core/result.h"

#include <string>

namespace vsr {

std::string format_duration_seconds(double duration_seconds);
Result<MediaProbeSummary> probe_media_for_ui(const std::string& path);

} // namespace vsr
```

Create `backend/src/video/media_probe_service.cpp`:

```cpp
#include "video/media_probe_service.h"

#if defined(VSR_ENABLE_FFMPEG)
#include "video/ffmpeg/ffmpeg_probe.h"
#endif

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace vsr {

std::string format_duration_seconds(double duration_seconds) {
    if (duration_seconds <= 0.0) {
        return "";
    }
    const auto rounded = static_cast<int>(std::llround(duration_seconds));
    const int hours = rounded / 3600;
    const int minutes = (rounded % 3600) / 60;
    const int seconds = rounded % 60;

    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << hours << ":"
           << std::setw(2) << minutes << ":"
           << std::setw(2) << seconds;
    return stream.str();
}

Result<MediaProbeSummary> probe_media_for_ui(const std::string& path) {
    if (path.empty()) {
        return Result<MediaProbeSummary>::Fail({"input_path_required", "Input path is required.", ""});
    }

    std::filesystem::path input(path);
    std::error_code error;
    if (!std::filesystem::exists(input, error)) {
        return Result<MediaProbeSummary>::Fail({"input_not_found", "Input file was not found.", path});
    }
    if (!std::filesystem::is_regular_file(input, error)) {
        return Result<MediaProbeSummary>::Fail({"input_not_file", "Input path is not a file.", path});
    }

    MediaProbeSummary summary;
    summary.path = path;
    summary.name = input.filename().string();
    summary.size_bytes = static_cast<std::uint64_t>(std::filesystem::file_size(input, error));
    if (error) {
        summary.size_bytes = 0;
        summary.warnings.push_back("File size could not be read.");
    }

#if defined(VSR_ENABLE_FFMPEG)
    const auto detailed = probe_media(path);
    if (!detailed.ok()) {
        return Result<MediaProbeSummary>::Fail(detailed.error());
    }
    const auto& media = detailed.value();
    if (media.width > 0 && media.height > 0) {
        summary.resolution = std::to_string(media.width) + "x" + std::to_string(media.height);
    }
    summary.duration = format_duration_seconds(media.duration_seconds);
    summary.codec = media.codec_name;
    if (media.bit_depth > 0) {
        summary.codec += " " + std::to_string(media.bit_depth) + "-bit";
    }
    summary.warnings.insert(summary.warnings.end(), media.warnings.begin(), media.warnings.end());
#else
    summary.warnings.push_back("FFmpeg support is not enabled; detailed media metadata is unavailable.");
#endif

    return Result<MediaProbeSummary>::Ok(std::move(summary));
}

} // namespace vsr
```

- [ ] **Step 5: Compile media probe service**

Add `src/video/media_probe_service.cpp` to `_vsr_core_sources` in `backend/CMakeLists.txt`:

```cmake
set(_vsr_core_sources
  src/core/time.cpp
  src/jobs/job_store.cpp
  src/jobs/job_runner.cpp
  src/video/fake_pipeline.cpp
  src/video/media_probe_service.cpp
  src/api/json_dto.cpp
  src/api/http_server.cpp
  src/platform/capabilities.cpp
  src/platform/logging.cpp
)
```

- [ ] **Step 6: Add CORS and media probe route**

In `backend/src/api/http_server.cpp`, add:

```cpp
#include "video/media_probe_service.h"
```

At the start of `HttpServer::bind_routes()`, add:

```cpp
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
    });

    server_.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& response) {
        response.status = 204;
    });
```

After `/api/capabilities`, add:

```cpp
    server_.Post("/api/media/probe", [](const httplib::Request& request, httplib::Response& response) {
        const auto body = nlohmann::json::parse(request.body, nullptr, false);
        if (body.is_discarded()) {
            response.status = 400;
            set_json(response, error_to_json({"invalid_json", "Request JSON is invalid.", ""}));
            return;
        }

        const auto parsed = parse_media_probe_request(body);
        if (!parsed.ok()) {
            response.status = 400;
            set_json(response, error_to_json(parsed.error()));
            return;
        }

        const auto probed = probe_media_for_ui(parsed.value().input_path);
        if (!probed.ok()) {
            response.status = 400;
            set_json(response, error_to_json(probed.error()));
            return;
        }

        set_json(response, media_probe_summary_to_json(probed.value()));
    });
```

- [ ] **Step 7: Run backend verification**

Run:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

Expected: all backend tests pass.

- [ ] **Step 8: Commit**

```powershell
git add backend/src/api/http_server.cpp backend/src/api/json_dto.h backend/src/api/json_dto.cpp backend/src/video/ffmpeg/ffmpeg_probe.h backend/src/video/ffmpeg/ffmpeg_probe.cpp backend/src/video/media_probe_service.h backend/src/video/media_probe_service.cpp backend/CMakeLists.txt
git commit -m "feat: add media probe backend endpoint"
```

---

### Task 4: Frontend DTO Types And Job Request Builder

**Files:**

- Create: `frontend/rtx-video-converter/src/api/types.ts`
- Create: `frontend/rtx-video-converter/src/lib/jobRequest.ts`
- Create: `frontend/rtx-video-converter/src/lib/jobRequest.test.ts`
- Modify: `frontend/rtx-video-converter/src/types.ts`

- [ ] **Step 1: Add frontend/backend DTO types**

Create `src/api/types.ts`:

```ts
export type ProcessingMode = 'vsr' | 'hdr' | 'both';
export type UiCodec = 'h264' | 'hevc';
export type JobState = 'queued' | 'running' | 'succeeded' | 'failed' | 'canceling' | 'canceled';
export type JobStage =
  | 'validating'
  | 'probing'
  | 'initializing_gpu'
  | 'decoding'
  | 'processing_rtx'
  | 'encoding'
  | 'muxing'
  | 'finalizing';

export interface BackendErrorBody {
  error: {
    code: string;
    message: string;
    details: string;
  };
}

export interface HealthResponse {
  version: string;
  ready: boolean;
}

export interface CapabilityResponse {
  d3d11Available: boolean;
  rtxSdkFound: boolean;
  vsrAvailable: boolean;
  truehdrAvailable: boolean;
  nvencH264Available: boolean;
  nvencHevcMain10Available: boolean;
  messages: string[];
}

export interface MediaProbeResponse {
  path: string;
  name: string;
  sizeBytes: number;
  resolution: string;
  duration: string;
  codec: string;
  warnings: string[];
}

export interface TranscodeRequest {
  inputPath: string;
  outputPath: string;
  processing: {
    vsr: {
      enabled: boolean;
      quality: number;
      scale: number;
    };
    hdr: {
      enabled: boolean;
      contrast: number;
      saturation: number;
      middleGray: number;
      maxLuminance: number;
    };
  };
  output: {
    container: 'mp4';
    videoCodec: UiCodec;
    audioMode: 'copy' | 'none';
    subtitleMode: 'copy-compatible' | 'none';
  };
}

export interface CreateJobResponse {
  id: string;
}

export interface JobSnapshot {
  id: string;
  state: JobState;
  stage: JobStage;
  progress: number;
  framesDone: number;
  framesTotal: number;
  fps: number;
  etaSeconds: number;
  inputPath: string;
  outputPath: string;
  warnings: string[];
  error?: {
    code: string;
    message: string;
    details: string;
  };
}
```

- [ ] **Step 2: Replace `src/types.ts` with UI settings types**

Use this complete file:

```ts
import type { MediaProbeResponse, ProcessingMode, UiCodec } from './api/types';

export type { MediaProbeResponse, ProcessingMode, UiCodec };

export interface ConversionSettings {
  mode: ProcessingMode;
  vsrScale: number;
  vsrQuality: number;
  hdrContrast: number;
  hdrSaturation: number;
  hdrMiddleGray: number;
  hdrMaxLuminance: number;
  codec: UiCodec;
  keepAudio: boolean;
  keepSubtitles: boolean;
}

export interface SelectedInput {
  path: string;
  metadata: MediaProbeResponse | null;
}
```

- [ ] **Step 3: Write request builder tests**

Create `src/lib/jobRequest.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import { buildOutputPath, buildTranscodeRequest, defaultSettings } from './jobRequest';

describe('buildOutputPath', () => {
  it('uses RTX suffix and mp4 extension', () => {
    expect(buildOutputPath('C:\\Videos\\input.mkv', 'D:\\Output')).toBe('D:\\Output\\input_RTX.mp4');
  });

  it('preserves a trailing slash on the output directory', () => {
    expect(buildOutputPath('C:\\Videos\\input.mp4', 'D:\\Output\\')).toBe('D:\\Output\\input_RTX.mp4');
  });
});

describe('buildTranscodeRequest', () => {
  it('builds VSR-only h264 request', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, mode: 'vsr', codec: 'h264' },
    });

    expect(request.processing.vsr.enabled).toBe(true);
    expect(request.processing.hdr.enabled).toBe(false);
    expect(request.output.videoCodec).toBe('h264');
    expect(request.output.audioMode).toBe('copy');
    expect(request.output.subtitleMode).toBe('copy-compatible');
  });

  it('forces hevc when HDR is enabled', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, mode: 'both', codec: 'h264' },
    });

    expect(request.processing.vsr.enabled).toBe(true);
    expect(request.processing.hdr.enabled).toBe(true);
    expect(request.output.videoCodec).toBe('hevc');
  });

  it('maps disabled audio and subtitles to none', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, keepAudio: false, keepSubtitles: false },
    });

    expect(request.output.audioMode).toBe('none');
    expect(request.output.subtitleMode).toBe('none');
  });
});
```

- [ ] **Step 4: Run tests and verify failure**

Run:

```powershell
cd frontend\rtx-video-converter
npm run test -- jobRequest
```

Expected: tests fail because `jobRequest.ts` does not exist.

- [ ] **Step 5: Implement request builder**

Create `src/lib/jobRequest.ts`:

```ts
import type { TranscodeRequest } from '../api/types';
import type { ConversionSettings } from '../types';

export const defaultSettings: ConversionSettings = {
  mode: 'both',
  vsrScale: 2,
  vsrQuality: 4,
  hdrContrast: 100,
  hdrSaturation: 100,
  hdrMiddleGray: 44,
  hdrMaxLuminance: 1000,
  codec: 'hevc',
  keepAudio: true,
  keepSubtitles: true,
};

export function filenameStem(path: string): string {
  const normalized = path.replaceAll('/', '\\');
  const name = normalized.split('\\').filter(Boolean).at(-1) ?? 'output';
  return name.replace(/\.[^/.\\]+$/, '');
}

export function directoryName(path: string): string {
  const normalized = path.replaceAll('/', '\\');
  const index = normalized.lastIndexOf('\\');
  return index >= 0 ? normalized.slice(0, index) : '';
}

export function buildOutputPath(inputPath: string, outputDirectory: string): string {
  const separator = outputDirectory.endsWith('\\') || outputDirectory.endsWith('/') ? '' : '\\';
  return `${outputDirectory}${separator}${filenameStem(inputPath)}_RTX.mp4`;
}

export function buildTranscodeRequest({
  inputPath,
  outputDirectory,
  settings,
}: {
  inputPath: string;
  outputDirectory: string;
  settings: ConversionSettings;
}): TranscodeRequest {
  const hdrEnabled = settings.mode === 'hdr' || settings.mode === 'both';
  const vsrEnabled = settings.mode === 'vsr' || settings.mode === 'both';

  return {
    inputPath,
    outputPath: buildOutputPath(inputPath, outputDirectory),
    processing: {
      vsr: {
        enabled: vsrEnabled,
        quality: settings.vsrQuality,
        scale: settings.vsrScale,
      },
      hdr: {
        enabled: hdrEnabled,
        contrast: settings.hdrContrast,
        saturation: settings.hdrSaturation,
        middleGray: settings.hdrMiddleGray,
        maxLuminance: settings.hdrMaxLuminance,
      },
    },
    output: {
      container: 'mp4',
      videoCodec: hdrEnabled ? 'hevc' : settings.codec,
      audioMode: settings.keepAudio ? 'copy' : 'none',
      subtitleMode: settings.keepSubtitles ? 'copy-compatible' : 'none',
    },
  };
}
```

- [ ] **Step 6: Run frontend tests**

Run:

```powershell
cd frontend\rtx-video-converter
npm run test -- jobRequest
npm run lint
```

Expected: request builder tests and TypeScript pass.

- [ ] **Step 7: Commit**

```powershell
git add frontend/rtx-video-converter/src/api/types.ts frontend/rtx-video-converter/src/types.ts frontend/rtx-video-converter/src/lib/jobRequest.ts frontend/rtx-video-converter/src/lib/jobRequest.test.ts
git commit -m "feat: add frontend job request mapping"
```

---

### Task 5: Frontend Backend Client

**Files:**

- Create: `frontend/rtx-video-converter/src/api/backendClient.ts`
- Create: `frontend/rtx-video-converter/src/api/backendClient.test.ts`

- [ ] **Step 1: Write backend client tests**

Create `src/api/backendClient.test.ts`:

```ts
import { afterEach, describe, expect, it, vi } from 'vitest';
import { ApiError, BackendClient } from './backendClient';

const originalFetch = globalThis.fetch;

afterEach(() => {
  globalThis.fetch = originalFetch;
  vi.restoreAllMocks();
});

describe('BackendClient', () => {
  it('loads health', async () => {
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ version: '0.1.0', ready: true }))) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');
    await expect(client.getHealth()).resolves.toEqual({ version: '0.1.0', ready: true });
  });

  it('parses backend error envelopes', async () => {
    globalThis.fetch = vi.fn(async () =>
      new Response(JSON.stringify({ error: { code: 'invalid_json', message: 'Bad JSON', details: 'line 1' } }), {
        status: 400,
      }),
    ) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');

    await expect(client.getJob('missing')).rejects.toMatchObject({
      code: 'invalid_json',
      message: 'Bad JSON',
      details: 'line 1',
      status: 400,
    });
  });

  it('turns network failures into ApiError', async () => {
    globalThis.fetch = vi.fn(async () => {
      throw new TypeError('fetch failed');
    }) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');

    await expect(client.getHealth()).rejects.toBeInstanceOf(ApiError);
    await expect(client.getHealth()).rejects.toMatchObject({ code: 'network_error' });
  });
});
```

- [ ] **Step 2: Run tests and verify failure**

Run:

```powershell
cd frontend\rtx-video-converter
npm run test -- backendClient
```

Expected: tests fail because `backendClient.ts` does not exist.

- [ ] **Step 3: Implement backend client**

Create `src/api/backendClient.ts`:

```ts
import type {
  BackendErrorBody,
  CapabilityResponse,
  CreateJobResponse,
  HealthResponse,
  JobSnapshot,
  MediaProbeResponse,
  TranscodeRequest,
} from './types';

export class ApiError extends Error {
  readonly code: string;
  readonly details: string;
  readonly status: number;

  constructor({ code, message, details, status }: { code: string; message: string; details: string; status: number }) {
    super(message);
    this.name = 'ApiError';
    this.code = code;
    this.details = details;
    this.status = status;
  }
}

async function readJson(response: Response): Promise<unknown> {
  const text = await response.text();
  return text.length > 0 ? JSON.parse(text) : {};
}

function isErrorBody(value: unknown): value is BackendErrorBody {
  return (
    typeof value === 'object' &&
    value !== null &&
    'error' in value &&
    typeof (value as BackendErrorBody).error?.code === 'string'
  );
}

export class BackendClient {
  constructor(private readonly baseUrl = import.meta.env.VITE_BACKEND_BASE_URL ?? 'http://127.0.0.1:49321') {}

  async getHealth(): Promise<HealthResponse> {
    return this.request('/api/health');
  }

  async getCapabilities(): Promise<CapabilityResponse> {
    return this.request('/api/capabilities');
  }

  async probeMedia(inputPath: string): Promise<MediaProbeResponse> {
    return this.request('/api/media/probe', {
      method: 'POST',
      body: JSON.stringify({ inputPath }),
    });
  }

  async createJob(request: TranscodeRequest): Promise<CreateJobResponse> {
    return this.request('/api/jobs', {
      method: 'POST',
      body: JSON.stringify(request),
    });
  }

  async getJob(id: string): Promise<JobSnapshot> {
    return this.request(`/api/jobs/${encodeURIComponent(id)}`);
  }

  async cancelJob(id: string): Promise<{ accepted: boolean }> {
    return this.request(`/api/jobs/${encodeURIComponent(id)}/cancel`, {
      method: 'POST',
    });
  }

  private async request<T>(path: string, init: RequestInit = {}): Promise<T> {
    let response: Response;
    try {
      response = await fetch(`${this.baseUrl}${path}`, {
        ...init,
        headers: {
          'Content-Type': 'application/json',
          ...init.headers,
        },
      });
    } catch (error) {
      throw new ApiError({
        code: 'network_error',
        message: 'Backend is not reachable.',
        details: error instanceof Error ? error.message : String(error),
        status: 0,
      });
    }

    const json = await readJson(response);
    if (!response.ok) {
      if (isErrorBody(json)) {
        throw new ApiError({ ...json.error, status: response.status });
      }
      throw new ApiError({
        code: 'http_error',
        message: `Backend returned HTTP ${response.status}.`,
        details: JSON.stringify(json),
        status: response.status,
      });
    }

    return json as T;
  }
}

export const backendClient = new BackendClient();
```

- [ ] **Step 4: Run frontend tests**

Run:

```powershell
cd frontend\rtx-video-converter
npm run test -- backendClient
npm run lint
```

Expected: backend client tests and TypeScript pass.

- [ ] **Step 5: Commit**

```powershell
git add frontend/rtx-video-converter/src/api/backendClient.ts frontend/rtx-video-converter/src/api/backendClient.test.ts
git commit -m "feat: add typed backend client"
```

---

### Task 6: Tauri Bridge And App Shell Scaffold

**Files:**

- Create: `frontend/rtx-video-converter/src/lib/tauriBridge.ts`
- Create: `frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1`
- Create: `frontend/rtx-video-converter/src-tauri/Cargo.toml`
- Create: `frontend/rtx-video-converter/src-tauri/build.rs`
- Create: `frontend/rtx-video-converter/src-tauri/src/main.rs`
- Create: `frontend/rtx-video-converter/src-tauri/src/lib.rs`
- Create: `frontend/rtx-video-converter/src-tauri/tauri.conf.json`
- Create: `frontend/rtx-video-converter/src-tauri/capabilities/default.json`
- Create: `frontend/rtx-video-converter/src-tauri/binaries/.gitkeep`

- [ ] **Step 1: Implement Tauri bridge**

Create `src/lib/tauriBridge.ts`:

```ts
type SidecarChild = {
  kill: () => Promise<void>;
};

let backendChild: SidecarChild | null = null;

export function isTauriRuntime(): boolean {
  return typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
}

export async function startBackendSidecar(port = 49321): Promise<{ runtime: 'tauri' | 'browser'; started: boolean }> {
  if (!isTauriRuntime()) {
    return { runtime: 'browser', started: false };
  }
  if (backendChild !== null) {
    return { runtime: 'tauri', started: true };
  }

  const { Command } = await import('@tauri-apps/plugin-shell');
  const command = Command.sidecar('binaries/vsr_backend', ['--port', String(port)]);
  backendChild = (await command.spawn()) as SidecarChild;
  return { runtime: 'tauri', started: true };
}

export async function stopBackendSidecar(): Promise<void> {
  if (backendChild === null) {
    return;
  }
  const child = backendChild;
  backendChild = null;
  await child.kill();
}

export async function pickInputVideo(): Promise<string | null> {
  if (!isTauriRuntime()) {
    return null;
  }
  const { open } = await import('@tauri-apps/plugin-dialog');
  const selected = await open({
    multiple: false,
    directory: false,
    filters: [
      {
        name: 'Video',
        extensions: ['mp4', 'mkv', 'mov', 'avi', 'webm'],
      },
    ],
  });
  return typeof selected === 'string' ? selected : null;
}

export async function pickOutputDirectory(): Promise<string | null> {
  if (!isTauriRuntime()) {
    return null;
  }
  const { open } = await import('@tauri-apps/plugin-dialog');
  const selected = await open({
    multiple: false,
    directory: true,
  });
  return typeof selected === 'string' ? selected : null;
}
```

- [ ] **Step 2: Add sidecar preparation script**

Create `scripts/prepare-tauri-sidecar.ps1`:

```powershell
param(
  [string]$BackendExe = "..\..\build\backend\Debug\vsr_backend.exe"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BackendPath = Resolve-Path (Join-Path $ProjectRoot $BackendExe)
$BinariesDir = Join-Path $ProjectRoot "src-tauri\binaries"

if (-not (Get-Command rustc -ErrorAction SilentlyContinue)) {
  throw "rustc is required to prepare a Tauri sidecar binary. Install Rust, then rerun this script."
}

$TargetTriple = (& rustc --print host-tuple).Trim()
if ([string]::IsNullOrWhiteSpace($TargetTriple)) {
  throw "rustc did not return a host target triple."
}

New-Item -ItemType Directory -Force -Path $BinariesDir | Out-Null
$Destination = Join-Path $BinariesDir "vsr_backend-$TargetTriple.exe"
Copy-Item -LiteralPath $BackendPath -Destination $Destination -Force
Write-Host "Prepared sidecar: $Destination"
```

- [ ] **Step 3: Add Tauri Rust files**

Create `src-tauri/Cargo.toml`:

```toml
[package]
name = "rtx-video-converter"
version = "0.1.0"
description = "RTX Video Converter"
authors = ["RTX Video Converter Contributors"]
edition = "2021"

[lib]
name = "rtx_video_converter_lib"
crate-type = ["staticlib", "cdylib", "rlib"]

[build-dependencies]
tauri-build = { version = "2", features = [] }

[dependencies]
tauri = { version = "2", features = [] }
tauri-plugin-dialog = "2"
tauri-plugin-shell = "2"
```

Create `src-tauri/build.rs`:

```rust
fn main() {
    tauri_build::build()
}
```

Create `src-tauri/src/main.rs`:

```rust
fn main() {
    rtx_video_converter_lib::run()
}
```

Create `src-tauri/src/lib.rs`:

```rust
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_shell::init())
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

- [ ] **Step 4: Add Tauri config and capabilities**

Create `src-tauri/tauri.conf.json`:

```json
{
  "$schema": "https://schema.tauri.app/config/2",
  "productName": "RTX Video Converter",
  "version": "0.1.0",
  "identifier": "com.vsr.rtx-video-converter",
  "build": {
    "beforeDevCommand": "npm run dev",
    "devUrl": "http://127.0.0.1:3000",
    "beforeBuildCommand": "npm run build",
    "frontendDist": "../dist"
  },
  "app": {
    "windows": [
      {
        "title": "RTX Video Converter",
        "width": 1280,
        "height": 820,
        "minWidth": 960,
        "minHeight": 680
      }
    ]
  },
  "bundle": {
    "active": true,
    "targets": ["nsis"],
    "externalBin": ["binaries/vsr_backend"]
  }
}
```

Create `src-tauri/capabilities/default.json`:

```json
{
  "$schema": "../gen/schemas/desktop-schema.json",
  "identifier": "main-capability",
  "description": "Capability for the main window",
  "windows": ["main"],
  "permissions": [
    "core:default",
    "dialog:allow-open",
    {
      "identifier": "shell:allow-spawn",
      "allow": [
        {
          "name": "binaries/vsr_backend",
          "sidecar": true,
          "args": ["--port", { "validator": "\\d{2,5}" }]
        }
      ]
    },
    {
      "identifier": "shell:allow-kill",
      "allow": [
        {
          "name": "binaries/vsr_backend",
          "sidecar": true,
          "args": true
        }
      ]
    }
  ]
}
```

Create `src-tauri/binaries/.gitkeep` as an empty file.

- [ ] **Step 5: Run frontend build**

Run:

```powershell
cd frontend\rtx-video-converter
npm run lint
npm run build
```

Expected: TypeScript and Vite build pass.

- [ ] **Step 6: Check Tauri prerequisite status**

Run:

```powershell
rustc --version
cargo --version
```

Expected: if Rust is installed, both commands print versions. If they are not installed, record that `npm run tauri:dev` and `npm run tauri:build` cannot be verified until Rust is installed.

- [ ] **Step 7: Commit**

```powershell
git add frontend/rtx-video-converter/src/lib/tauriBridge.ts frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1 frontend/rtx-video-converter/src-tauri
git commit -m "feat: add tauri shell bridge"
```

---

### Task 7: Frontend Backend Status And Job Hooks

**Files:**

- Create: `frontend/rtx-video-converter/src/hooks/useBackendStatus.ts`
- Create: `frontend/rtx-video-converter/src/hooks/useTranscodeJob.ts`

- [ ] **Step 1: Implement backend status hook**

Create `src/hooks/useBackendStatus.ts`:

```ts
import { useEffect, useState } from 'react';
import { backendClient } from '../api/backendClient';
import type { CapabilityResponse, HealthResponse } from '../api/types';
import { startBackendSidecar } from '../lib/tauriBridge';

export type BackendStatus = 'starting' | 'ready' | 'offline' | 'degraded';

export interface BackendStatusState {
  status: BackendStatus;
  runtime: 'tauri' | 'browser';
  health: HealthResponse | null;
  capabilities: CapabilityResponse | null;
  message: string;
  refresh: () => Promise<void>;
}

async function waitForHealth(): Promise<HealthResponse> {
  let lastError: unknown;
  for (let attempt = 0; attempt < 20; ++attempt) {
    try {
      return await backendClient.getHealth();
    } catch (error) {
      lastError = error;
      await new Promise((resolve) => window.setTimeout(resolve, 250));
    }
  }
  throw lastError;
}

export function useBackendStatus(): BackendStatusState {
  const [status, setStatus] = useState<BackendStatus>('starting');
  const [runtime, setRuntime] = useState<'tauri' | 'browser'>('browser');
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [capabilities, setCapabilities] = useState<CapabilityResponse | null>(null);
  const [message, setMessage] = useState('正在启动后端...');

  const refresh = async () => {
    setStatus('starting');
    setMessage('正在连接后端...');
    try {
      const sidecar = await startBackendSidecar();
      setRuntime(sidecar.runtime);
      const nextHealth = await waitForHealth();
      const nextCapabilities = await backendClient.getCapabilities();
      setHealth(nextHealth);
      setCapabilities(nextCapabilities);
      const degraded = nextCapabilities.messages.length > 0;
      setStatus(degraded ? 'degraded' : 'ready');
      setMessage(degraded ? '后端已连接，但部分能力不可用。' : '后端已就绪。');
    } catch (error) {
      setHealth(null);
      setCapabilities(null);
      setStatus('offline');
      setMessage(error instanceof Error ? error.message : '后端未连接。');
    }
  };

  useEffect(() => {
    void refresh();
  }, []);

  return { status, runtime, health, capabilities, message, refresh };
}
```

- [ ] **Step 2: Implement transcode job hook**

Create `src/hooks/useTranscodeJob.ts`:

```ts
import { useEffect, useRef, useState } from 'react';
import { ApiError, backendClient } from '../api/backendClient';
import type { JobSnapshot, TranscodeRequest } from '../api/types';

const ACTIVE_STATES = new Set(['queued', 'running', 'canceling']);

export interface TranscodeJobState {
  activeJob: JobSnapshot | null;
  submitting: boolean;
  canceling: boolean;
  error: ApiError | null;
  startJob: (request: TranscodeRequest) => Promise<void>;
  cancelJob: () => Promise<void>;
  clearJob: () => void;
}

export function useTranscodeJob(): TranscodeJobState {
  const [activeJob, setActiveJob] = useState<JobSnapshot | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const [canceling, setCanceling] = useState(false);
  const [error, setError] = useState<ApiError | null>(null);
  const activeJobId = useRef<string | null>(null);

  const startJob = async (request: TranscodeRequest) => {
    setSubmitting(true);
    setError(null);
    try {
      const created = await backendClient.createJob(request);
      activeJobId.current = created.id;
      setActiveJob({
        id: created.id,
        state: 'queued',
        stage: 'validating',
        progress: 0,
        framesDone: 0,
        framesTotal: 0,
        fps: 0,
        etaSeconds: 0,
        inputPath: request.inputPath,
        outputPath: request.outputPath,
        warnings: [],
      });
    } catch (nextError) {
      setError(nextError instanceof ApiError ? nextError : new ApiError({
        code: 'job_submit_failed',
        message: 'Job submission failed.',
        details: String(nextError),
        status: 0,
      }));
    } finally {
      setSubmitting(false);
    }
  };

  const cancelJob = async () => {
    if (activeJobId.current === null) {
      return;
    }
    setCanceling(true);
    try {
      await backendClient.cancelJob(activeJobId.current);
    } catch (nextError) {
      if (nextError instanceof ApiError && nextError.status === 409) {
        const snapshot = await backendClient.getJob(activeJobId.current);
        setActiveJob(snapshot);
      } else {
        setError(nextError instanceof ApiError ? nextError : null);
      }
    } finally {
      setCanceling(false);
    }
  };

  const clearJob = () => {
    activeJobId.current = null;
    setActiveJob(null);
    setError(null);
    setCanceling(false);
    setSubmitting(false);
  };

  useEffect(() => {
    if (activeJobId.current === null) {
      return;
    }
    const interval = window.setInterval(async () => {
      if (activeJobId.current === null) {
        return;
      }
      try {
        const snapshot = await backendClient.getJob(activeJobId.current);
        setActiveJob(snapshot);
        if (!ACTIVE_STATES.has(snapshot.state)) {
          activeJobId.current = null;
        }
      } catch (nextError) {
        setError(nextError instanceof ApiError ? nextError : null);
      }
    }, 500);

    return () => window.clearInterval(interval);
  }, [activeJob?.id]);

  return { activeJob, submitting, canceling, error, startJob, cancelJob, clearJob };
}
```

- [ ] **Step 3: Run frontend verification**

Run:

```powershell
cd frontend\rtx-video-converter
npm run lint
npm run test
```

Expected: TypeScript and Vitest pass.

- [ ] **Step 4: Commit**

```powershell
git add frontend/rtx-video-converter/src/hooks/useBackendStatus.ts frontend/rtx-video-converter/src/hooks/useTranscodeJob.ts
git commit -m "feat: add backend status and job hooks"
```

---

### Task 8: React UI Component Split And Real Data Wiring

**Files:**

- Create: `frontend/rtx-video-converter/src/components/CapabilityBanner.tsx`
- Create: `frontend/rtx-video-converter/src/components/ErrorDetails.tsx`
- Create: `frontend/rtx-video-converter/src/components/InputPanel.tsx`
- Create: `frontend/rtx-video-converter/src/components/SettingsPanel.tsx`
- Create: `frontend/rtx-video-converter/src/components/StatusFooter.tsx`
- Modify: `frontend/rtx-video-converter/src/App.tsx`

- [ ] **Step 1: Add capability banner**

Create `src/components/CapabilityBanner.tsx`:

```tsx
import type { CapabilityResponse } from '../api/types';

export function CapabilityBanner({ capabilities, message }: { capabilities: CapabilityResponse | null; message: string }) {
  const messages = capabilities?.messages ?? [];
  return (
    <div className="rounded-xl border border-[#e5e5e5] bg-white p-4 shadow-sm">
      <div className="flex items-center justify-between gap-4">
        <div>
          <p className="text-sm font-semibold text-[#1a1a1a]">后端状态</p>
          <p className="text-xs text-[#5a5a5a]">{message}</p>
        </div>
        <div className="text-[11px] text-[#5a5a5a]">
          VSR {capabilities?.vsrAvailable ? '可用' : '不可用'} / HDR {capabilities?.truehdrAvailable ? '可用' : '不可用'}
        </div>
      </div>
      {messages.length > 0 && (
        <ul className="mt-3 space-y-1 text-xs text-[#8a5a00]">
          {messages.map((item) => (
            <li key={item}>{item}</li>
          ))}
        </ul>
      )}
    </div>
  );
}
```

- [ ] **Step 2: Add error details**

Create `src/components/ErrorDetails.tsx`:

```tsx
import type { ApiError } from '../api/backendClient';

export function ErrorDetails({ error }: { error: ApiError | null }) {
  if (error === null) {
    return null;
  }
  return (
    <details className="rounded-lg border border-red-200 bg-red-50 p-3 text-xs text-red-800">
      <summary className="cursor-pointer font-semibold">{error.message}</summary>
      <pre className="mt-2 whitespace-pre-wrap break-words">{error.details || error.code}</pre>
    </details>
  );
}
```

- [ ] **Step 3: Add input panel**

Create `src/components/InputPanel.tsx`:

```tsx
import type { MediaProbeResponse } from '../api/types';

export function InputPanel({
  inputPath,
  metadata,
  outputDirectory,
  outputPath,
  onBrowseFile,
  onBrowseOutput,
  onDropPath,
  onOutputDirectoryChange,
}: {
  inputPath: string;
  metadata: MediaProbeResponse | null;
  outputDirectory: string;
  outputPath: string;
  onBrowseFile: () => void;
  onBrowseOutput: () => void;
  onDropPath: (path: string) => void;
  onOutputDirectoryChange: (path: string) => void;
}) {
  return (
    <aside className="hidden w-80 shrink-0 flex-col gap-6 overflow-y-auto border-r border-[#e5e5e5] bg-white p-4 md:flex">
      <section>
        <h2 className="mb-3 text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a]">输入源</h2>
        <div
          className="mb-4 flex h-[120px] cursor-pointer flex-col items-center justify-center rounded-lg border-2 border-dashed border-[#d1d1d1] bg-[#fcfcfc] p-6 text-center transition-colors hover:border-[#aaa]"
          onClick={onBrowseFile}
          onDragOver={(event) => event.preventDefault()}
          onDrop={(event) => {
            event.preventDefault();
            const file = event.dataTransfer.files[0];
            if (file?.name) {
              onDropPath(file.name);
            }
          }}
        >
          <div className="mb-2 text-2xl">📂</div>
          <p className="text-xs text-[#5a5a5a]">
            拖拽视频至此或 <span className="text-[#0067c0] hover:underline">浏览</span>
          </p>
        </div>
        <div className="space-y-2 rounded border border-[#e5e5e5] bg-[#f9f9f9] p-3 text-[11px]">
          <div className="flex justify-between gap-3">
            <span>文件名:</span>
            <span className="truncate font-medium" title={metadata?.name ?? inputPath}>{metadata?.name ?? inputPath || '未选择'}</span>
          </div>
          <div className="flex justify-between">
            <span>大小:</span>
            <span className="font-medium">{metadata ? `${(metadata.sizeBytes / 1024 / 1024).toFixed(1)} MB` : '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>分辨率:</span>
            <span className="font-medium">{metadata?.resolution || '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>时长:</span>
            <span className="font-medium">{metadata?.duration || '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>编码:</span>
            <span className="font-medium text-[#0067c0]">{metadata?.codec || '-'}</span>
          </div>
        </div>
      </section>
      <section className="flex grow flex-col gap-4">
        <h2 className="text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a]">输出设置</h2>
        <div>
          <label className="mb-1 block text-xs">输出目录</label>
          <div className="flex gap-1">
            <input
              type="text"
              value={outputDirectory}
              onChange={(event) => onOutputDirectoryChange(event.target.value)}
              className="grow rounded border border-[#d1d1d1] bg-white px-2 py-1.5 text-xs shadow-sm focus:border-[#0067c0] focus:outline-none"
            />
            <button className="rounded border border-[#d1d1d1] bg-white px-3 text-xs hover:bg-[#f9f9f9]" onClick={onBrowseOutput}>
              ...
            </button>
          </div>
        </div>
        <div>
          <label className="mb-1 block text-xs">完整输出路径预览</label>
          <p className="break-all rounded border border-[#e5e5e5] bg-[#f9f9f9] p-2 text-[10px] italic text-[#5a5a5a]">{outputPath || '未生成'}</p>
        </div>
      </section>
    </aside>
  );
}
```

- [ ] **Step 4: Add settings panel and footer**

Move the existing VSR/HDR/encoder JSX from `App.tsx` into `SettingsPanel.tsx` with this prop contract:

```tsx
import { WinSegmentedControl, WinSlider, WinSwitch } from './WinUI';
import type { CapabilityResponse, ProcessingMode, UiCodec } from '../api/types';
import type { ConversionSettings } from '../types';

export function SettingsPanel({
  settings,
  capabilities,
  disabled,
  onChange,
}: {
  settings: ConversionSettings;
  capabilities: CapabilityResponse | null;
  disabled: boolean;
  onChange: (settings: ConversionSettings) => void;
}) {
  const hdrDisabled = disabled || capabilities?.truehdrAvailable === false;
  const vsrDisabled = disabled || capabilities?.vsrAvailable === false;
  const codecDisabled = disabled || settings.mode !== 'vsr';

  const update = (patch: Partial<ConversionSettings>) => onChange({ ...settings, ...patch });

  return (
    <section className="flex grow flex-col overflow-y-auto bg-[#f3f3f3]">
      <div className="w-full max-w-4xl space-y-6 p-6">
        <WinSegmentedControl
          options={[
            { label: 'VSR 超分', value: 'vsr', disabled: vsrDisabled },
            { label: 'HDR 映射', value: 'hdr', disabled: hdrDisabled },
            { label: 'VSR + HDR', value: 'both', disabled: vsrDisabled || hdrDisabled },
          ]}
          value={settings.mode}
          onChange={(value) => update({ mode: value as ProcessingMode, codec: value === 'vsr' ? settings.codec : 'hevc' })}
        />
        <div className="grid w-full grid-cols-1 gap-6 md:grid-cols-2">
          <div className={`rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm ${settings.mode === 'hdr' ? 'pointer-events-none opacity-40' : ''}`}>
            <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">VSR 参数</h3>
            <WinSlider label="提升倍率" value={settings.vsrScale} min={1} max={4} step={1} onChange={(vsrScale) => update({ vsrScale })} formatValue={(value) => `${value}.0x`} />
          </div>
          <div className={`rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm ${settings.mode === 'vsr' ? 'pointer-events-none opacity-40' : ''}`}>
            <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">HDR 参数</h3>
            <div className="grid grid-cols-2 gap-4">
              <input type="number" value={settings.hdrContrast} onChange={(event) => update({ hdrContrast: Number(event.target.value) })} className="rounded border border-[#d1d1d1] p-1.5 text-xs" />
              <input type="number" value={settings.hdrSaturation} onChange={(event) => update({ hdrSaturation: Number(event.target.value) })} className="rounded border border-[#d1d1d1] p-1.5 text-xs" />
              <input type="number" value={settings.hdrMiddleGray} onChange={(event) => update({ hdrMiddleGray: Number(event.target.value) })} className="rounded border border-[#d1d1d1] p-1.5 text-xs" />
              <input type="number" value={settings.hdrMaxLuminance} onChange={(event) => update({ hdrMaxLuminance: Number(event.target.value) })} className="rounded border border-[#d1d1d1] p-1.5 text-xs" />
            </div>
          </div>
        </div>
        <div className="rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm">
          <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">编码器与轨道</h3>
          <select value={settings.codec} onChange={(event) => update({ codec: event.target.value as UiCodec })} disabled={codecDisabled} className="rounded border border-[#d1d1d1] p-2 text-xs">
            <option value="h264" disabled={capabilities?.nvencH264Available === false}>NVENC H.264</option>
            <option value="hevc" disabled={capabilities?.nvencHevcMain10Available === false}>NVENC HEVC Main10</option>
          </select>
          <div className="mt-4 flex gap-4">
            <WinSwitch label="音频" checked={settings.keepAudio} onChange={(keepAudio) => update({ keepAudio })} />
            <WinSwitch label="字幕" checked={settings.keepSubtitles} onChange={(keepSubtitles) => update({ keepSubtitles })} />
          </div>
        </div>
      </div>
    </section>
  );
}
```

Create `StatusFooter.tsx` with this complete file:

```tsx
import type { JobSnapshot } from '../api/types';

export function StatusFooter({
  job,
  canStart,
  submitting,
  canceling,
  onStart,
  onCancel,
}: {
  job: JobSnapshot | null;
  canStart: boolean;
  submitting: boolean;
  canceling: boolean;
  onStart: () => void;
  onCancel: () => void;
}) {
  const active = job !== null && ['queued', 'running', 'canceling'].includes(job.state);
  const progress = job ? Math.round(job.progress * 100) : 0;
  const label = job ? `${job.state}: ${job.stage}` : '等待开始';

  return (
    <footer className="z-20 flex h-[84px] shrink-0 items-center gap-8 border-t border-[#e5e5e5] bg-white px-6 py-3 shadow-[0_-4px_12px_rgba(0,0,0,0.03)]">
      <div className="grow">
        <div className="mb-1.5 flex items-end justify-between">
          <div className="flex items-center gap-3">
            <span className={`text-sm font-bold ${active ? 'text-[#0067c0]' : 'text-[#a1a1a1]'}`}>{progress}%</span>
            <span className="text-xs font-medium text-[#1a1a1a]">状态: {label}</span>
            {job && <span className="text-xs text-[#5a5a5a]">{job.framesDone}/{job.framesTotal} frames · {job.fps.toFixed(1)} fps · ETA {job.etaSeconds}s</span>}
          </div>
        </div>
        <div className="h-1.5 w-full overflow-hidden rounded-full bg-[#eee]">
          <div className="h-full rounded-full bg-[#0067c0] transition-all duration-300 ease-out" style={{ width: `${progress}%` }} />
        </div>
      </div>
      <button
        onClick={active ? onCancel : onStart}
        disabled={active ? canceling : !canStart || submitting}
        className={`h-11 rounded px-10 text-sm font-semibold shadow-md transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
          active ? 'border border-[#d1d1d1] bg-white text-[#dc2626] hover:bg-black/5' : 'border border-transparent bg-[#0067c0] text-white hover:bg-[#005bb0]'
        }`}
      >
        {active ? (canceling ? '正在取消...' : '取消任务') : submitting ? '提交中...' : '开始转码'}
      </button>
    </footer>
  );
}
```

- [ ] **Step 5: Wire `App.tsx` to real backend state**

Replace simulated processing state in `App.tsx` with:

```tsx
const backend = useBackendStatus();
const job = useTranscodeJob();
const [selectedInput, setSelectedInput] = useState<SelectedInput>({ path: '', metadata: null });
const [outputDirectory, setOutputDirectory] = useState('');
const [settings, setSettings] = useState(defaultSettings);
const outputPath = selectedInput.path ? buildOutputPath(selectedInput.path, outputDirectory || directoryName(selectedInput.path)) : '';
const canStart = backend.status !== 'offline' && selectedInput.path.length > 0 && outputPath.length > 0;

const chooseInput = async () => {
  const path = await pickInputVideo();
  if (path !== null) {
    await loadInput(path);
  }
};

const loadInput = async (path: string) => {
  setSelectedInput({ path, metadata: null });
  if (outputDirectory.length === 0) {
    setOutputDirectory(directoryName(path));
  }
  const metadata = await backendClient.probeMedia(path);
  setSelectedInput({ path, metadata });
};

const start = async () => {
  await job.startJob(buildTranscodeRequest({
    inputPath: selectedInput.path,
    outputDirectory: outputDirectory || directoryName(selectedInput.path),
    settings,
  }));
};
```

Render:

```tsx
<InputPanel ... />
<section className="flex grow flex-col overflow-y-auto">
  <div className="p-6">
    <CapabilityBanner capabilities={backend.capabilities} message={backend.message} />
    <ErrorDetails error={job.error} />
  </div>
  <SettingsPanel settings={settings} capabilities={backend.capabilities} disabled={job.activeJob !== null} onChange={setSettings} />
</section>
<StatusFooter job={job.activeJob} canStart={canStart} submitting={job.submitting} canceling={job.canceling} onStart={start} onCancel={job.cancelJob} />
```

- [ ] **Step 6: Update `WinSegmentedControl` for disabled options**

Modify the options type in `WinUI.tsx`:

```tsx
options: { label: string; value: string; disabled?: boolean }[];
```

Add `disabled={opt.disabled}` to the button and include disabled classes:

```tsx
opt.disabled ? 'cursor-not-allowed opacity-40' : 'hover:bg-black/5 hover:text-[#1a1a1a]'
```

- [ ] **Step 7: Run frontend verification**

Run:

```powershell
cd frontend\rtx-video-converter
npm run lint
npm run build
npm run test
```

Expected: TypeScript, Vite build, and Vitest pass.

- [ ] **Step 8: Commit**

```powershell
git add frontend/rtx-video-converter/src/App.tsx frontend/rtx-video-converter/src/components frontend/rtx-video-converter/src/components/WinUI.tsx
git commit -m "feat: wire frontend to backend workflow"
```

---

### Task 9: Manual Integration And Tauri Verification

**Files:**

- Modify only files needed to fix failures found by the verification commands.

- [ ] **Step 1: Run complete backend verification**

Run:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

Expected: all backend tests pass and `vsr_backend` builds.

- [ ] **Step 2: Run complete frontend verification**

Run:

```powershell
cd frontend\rtx-video-converter
npm run lint
npm run test
npm run build
```

Expected: TypeScript, Vitest, and Vite build pass.

- [ ] **Step 3: Verify browser-mode backend integration**

Run backend:

```powershell
.\build\backend\Debug\vsr_backend.exe --port 49321
```

Run frontend in a second terminal:

```powershell
cd frontend\rtx-video-converter
npm run dev
```

Manual checks:

- Health and capabilities load without CORS errors.
- Capability messages appear.
- A manually supplied or dropped path calls `/api/media/probe`.
- `POST /api/jobs` succeeds with a valid file path.
- Polling updates progress from the backend fake pipeline.
- Cancel calls `/api/jobs/{id}/cancel`.

- [ ] **Step 4: Prepare Tauri sidecar when Rust is available**

Run:

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend\Debug\vsr_backend.exe
```

Expected: a file named like `src-tauri\binaries\vsr_backend-x86_64-pc-windows-msvc.exe` is created. If `rustc` is not installed, record that Tauri runtime verification is blocked by missing Rust.

- [ ] **Step 5: Run Tauri verification when Rust is available**

Run:

```powershell
cd frontend\rtx-video-converter
npm run tauri:dev
npm run tauri:build
```

Expected: Tauri dev launches the app, native file/folder dialogs work, sidecar starts, and build produces a Windows installer. If Rust is missing, skip this command and report the blocker.

- [ ] **Step 6: Commit verification fixes**

If verification required code fixes:

```powershell
git add frontend backend
git commit -m "fix: complete frontend integration verification"
```

If no fixes were required, do not create an empty commit.

---

## Self-Review

Spec coverage:

- Tauri v2 shell setup is covered by Task 6.
- Sidecar launch and binary preparation are covered by Task 6 and Task 9.
- Dialog file/folder selection is covered by Task 6.
- Real backend API integration is covered by Tasks 5, 7, and 8.
- Media probe endpoint is covered by Tasks 2 and 3.
- Capability-gated UI controls are covered by Task 8.
- Real job polling and cancellation are covered by Task 7.
- Error and warning display is covered by Tasks 5 and 8.
- AI Studio template cleanup is covered by Task 1.

Completion scan:

- The plan avoids undefined function names in implementation snippets.
- The plan uses concrete file paths, commands, and expected outcomes.
- The plan has no empty sections or postponed implementation steps.

Type consistency:

- Frontend `UiCodec` uses backend JSON values `h264` and `hevc`.
- Frontend HDR setting name `hdrMiddleGray` maps to backend JSON `middleGray`.
- Frontend HDR setting name `hdrMaxLuminance` maps to backend JSON `maxLuminance`.
- Media probe JSON uses `sizeBytes` in TypeScript and `size_bytes` in C++.
- Tauri sidecar name is consistently `binaries/vsr_backend`.
