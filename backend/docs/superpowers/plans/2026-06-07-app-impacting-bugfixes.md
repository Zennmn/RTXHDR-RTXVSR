# App-Impacting Bugfixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the bugs that affect the final packaged Windows app experience, not browser-only or standalone-backend edge cases.

**Architecture:** Treat the React/Tauri frontend and C++ backend as one desktop app with a bundled sidecar process. The app must own or intentionally reuse its backend, block unsupported conversions before submission, keep input metadata consistent, and avoid leaving damaged user output files. Packaging must always include the backend and runtime files that match the current build.

**Tech Stack:** C++20 backend with CMake/GTest, React 19 + TypeScript + Vitest frontend, Tauri 2 shell/dialog plugins, PowerShell packaging scripts.

---

## Scope

Fix these app-impacting bugs:

1. Stale `vsr_backend.exe` can remain after app crash/force-kill and a later app instance can silently connect to it.
2. Multiple app windows can start multiple sidecars while every window still targets fixed port `127.0.0.1:49321`.
3. The app can enable Start even when required VSR/HDR/NVENC capabilities are unavailable.
4. Manual input path changes do not re-probe metadata or update the default output directory.
5. Probe failures are not surfaced immediately and can leave a bad selected input in a startable state.
6. FFmpeg writes directly to the final output path, so cancel/failure/crash can leave damaged MP4 files or overwrite existing output.
7. Packaging can include a stale sidecar because frontend build scripts do not force sidecar preparation before `tauri build`.

Out of scope for this plan:

- Standalone backend CLI hardening, including invalid user-provided ports above 65535.
- Browser development mode polish beyond preserving a manual backend workflow.
- Full hardware transcode validation on every GPU/driver combination.

## File Structure

- Modify `backend/src/api/http_server.cpp` and `backend/src/api/http_server.h`
  - Add optional ownership/session API routes used by the packaged app to detect whether the backend is app-owned and whether it should shut down.
- Modify `backend/src/app/main.cpp`
  - Accept an optional app session token and optional parent PID/owner metadata. Keep default standalone behavior for development.
- Modify `backend/tests/unit/http_server_tests.cpp`
  - Cover app session health fields and owner shutdown route behavior.
- Modify `frontend/rtx-video-converter/src/api/types.ts`
  - Add app session fields to `HealthResponse`.
- Modify `frontend/rtx-video-converter/src/api/backendClient.ts`
  - Add a backend shutdown call for app-owned sidecars.
- Modify `frontend/rtx-video-converter/src/lib/tauriBridge.ts`
  - Generate a per-window session token, launch the sidecar with that token, verify health ownership, and stop only the sidecar owned by this window.
- Create `frontend/rtx-video-converter/src/lib/capabilities.ts`
  - Centralize conversion capability checks.
- Create `frontend/rtx-video-converter/src/lib/capabilities.test.ts`
  - Cover VSR/HDR/NVENC combinations used by Start gating.
- Modify `frontend/rtx-video-converter/src/App.tsx`
  - Use capability checks, re-probe manual paths, surface probe errors, and keep selection state consistent.
- Create or modify `frontend/rtx-video-converter/src/App.test.tsx`
  - Cover capability gating and manual path/probe behavior. If component testing dependencies are absent, add tests at hook/helper level instead and document the limitation in the task.
- Modify `backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`
  - Write to a temporary output path and atomically replace the destination after successful trailer write.
- Modify `backend/tests/unit/ffmpeg_transcode_pipeline_tests.cpp`
  - Add helper-level tests for temporary output path derivation and safe replace behavior.
- Modify `frontend/rtx-video-converter/package.json`
  - Make `tauri:build` and `tauri:build:portable` prepare the sidecar before packaging.
- Modify `frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1`
  - Default to the release backend path and fail loudly when the sidecar or runtime DLLs are missing.
- Modify `frontend/rtx-video-converter/README.md`
  - Update packaging command expectations.

---

### Task 1: App-Owned Backend Session Contract

**Files:**
- Modify: `backend/src/api/http_server.h`
- Modify: `backend/src/api/http_server.cpp`
- Modify: `backend/src/app/main.cpp`
- Modify: `backend/tests/unit/http_server_tests.cpp`
- Modify: `FRONTEND_INTEGRATION.md`

- [ ] **Step 1: Add failing HTTP tests for session-aware health and shutdown**

Add tests to `backend/tests/unit/http_server_tests.cpp` that create a server with an app session token and verify:

```cpp
TEST(HttpServer, healthIncludesAppSessionWhenConfigured) {
    TestHttpServer server("session-abc");
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Get("/api/health");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    const auto body = nlohmann::json::parse(response->body);
    EXPECT_EQ(body.at("version"), "0.1.0");
    EXPECT_EQ(body.at("ready"), true);
    EXPECT_EQ(body.at("appSessionId"), "session-abc");
}

TEST(HttpServer, shutdownRejectsMismatchedSession) {
    TestHttpServer server("session-abc");
    ASSERT_TRUE(server.start());
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Post("/api/app/shutdown", R"({"appSessionId":"wrong"})", "application/json");

    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 403);
    EXPECT_NE(response->body.find("app_session_mismatch"), std::string::npos);
}
```

Update the `TestHttpServer` fixture constructor so it can pass an optional session string to `HttpServer`.

- [ ] **Step 2: Run the backend test and verify it fails**

Run:

```powershell
cmake --build build/backend-audit --target vsr_backend_tests --config Debug
ctest --test-dir build/backend-audit --output-on-failure -C Debug -R "HttpServer.healthIncludesAppSessionWhenConfigured|HttpServer.shutdownRejectsMismatchedSession"
```

Expected: compilation or test failure because `HttpServer` has no session constructor and `/api/app/shutdown` does not exist.

- [ ] **Step 3: Add session configuration to `HttpServer`**

Introduce a small options type in `backend/src/api/http_server.h`:

```cpp
struct HttpServerOptions {
    std::string app_session_id;
};

class HttpServer {
public:
    HttpServer(JobStore& store, JobRunner& runner);
    HttpServer(JobStore& store, JobRunner& runner, HttpServerOptions options);
    ...
private:
    HttpServerOptions options_;
};
```

Keep the existing two-argument constructor delegating to the new constructor with default options.

- [ ] **Step 4: Implement session-aware health and shutdown route**

In `backend/src/api/http_server.cpp`, return `appSessionId` only when configured:

```cpp
server_.Get("/api/health", [this](const httplib::Request&, httplib::Response& response) {
    nlohmann::json body{{"version", "0.1.0"}, {"ready", true}};
    if (!options_.app_session_id.empty()) {
        body["appSessionId"] = options_.app_session_id;
    }
    set_json(response, body);
});
```

Add `POST /api/app/shutdown`:

```cpp
server_.Post("/api/app/shutdown", [this](const httplib::Request& request, httplib::Response& response) {
    if (options_.app_session_id.empty()) {
        response.status = 404;
        set_json(response, error_to_json({"app_shutdown_unavailable", "App-owned shutdown is not enabled.", ""}));
        return;
    }

    const auto body = nlohmann::json::parse(request.body, nullptr, false);
    if (body.is_discarded()) {
        response.status = 400;
        set_json(response, error_to_json({"invalid_json", "Request JSON is invalid.", ""}));
        return;
    }

    const auto session = body.value("appSessionId", "");
    if (session != options_.app_session_id) {
        response.status = 403;
        set_json(response, error_to_json({"app_session_mismatch", "Backend session does not belong to this app instance.", ""}));
        return;
    }

    set_json(response, {{"accepted", true}});
    std::thread([this] { stop(); }).detach();
});
```

- [ ] **Step 5: Add `--app-session-id` parsing**

In `backend/src/app/main.cpp`, parse `--app-session-id <value>` in addition to `--port`. Pass it to `HttpServerOptions`.

```cpp
struct AppArgs {
    int port = 49321;
    std::string app_session_id;
};
```

`parse_args()` must preserve current `--port` behavior and allow:

```powershell
vsr_backend.exe --port 49321 --app-session-id abc
```

- [ ] **Step 6: Run backend tests**

Run:

```powershell
cmake --build build/backend-audit --target vsr_backend_tests vsr_backend --config Debug
ctest --test-dir build/backend-audit --output-on-failure -C Debug
```

Expected: all backend tests pass.

- [ ] **Step 7: Update integration docs**

In `FRONTEND_INTEGRATION.md`, document that packaged app mode starts the backend with `--app-session-id`, verifies `/api/health.appSessionId`, and uses `/api/app/shutdown` only when the session matches.

- [ ] **Step 8: Commit**

```powershell
git add backend/src/api/http_server.h backend/src/api/http_server.cpp backend/src/app/main.cpp backend/tests/unit/http_server_tests.cpp FRONTEND_INTEGRATION.md
git commit -m "fix: add app-owned backend session contract"
```

---

### Task 2: Tauri Sidecar Ownership and Cleanup

**Files:**
- Modify: `frontend/rtx-video-converter/src/api/types.ts`
- Modify: `frontend/rtx-video-converter/src/api/backendClient.ts`
- Modify: `frontend/rtx-video-converter/src/api/backendClient.test.ts`
- Modify: `frontend/rtx-video-converter/src/lib/tauriBridge.ts`
- Modify: `frontend/rtx-video-converter/src/hooks/useBackendStatus.ts`
- Create: `frontend/rtx-video-converter/src/lib/tauriBridge.test.ts`

- [ ] **Step 1: Add failing API client test for app shutdown**

Add to `frontend/rtx-video-converter/src/api/backendClient.test.ts`:

```typescript
it('requests app-owned backend shutdown with the session id', async () => {
  const fetchMock = vi.fn(async () => new Response(JSON.stringify({ accepted: true }))) as unknown as typeof fetch;
  globalThis.fetch = fetchMock;

  const client = new BackendClient('http://127.0.0.1:49321');
  await expect(client.shutdownAppBackend('session-abc')).resolves.toEqual({ accepted: true });

  expect(fetchMock).toHaveBeenCalledWith(
    'http://127.0.0.1:49321/api/app/shutdown',
    expect.objectContaining({
      method: 'POST',
      body: JSON.stringify({ appSessionId: 'session-abc' }),
    }),
  );
});
```

- [ ] **Step 2: Run the frontend test and verify it fails**

Run:

```powershell
npm test -- backendClient.test.ts
```

Expected: FAIL because `shutdownAppBackend` does not exist.

- [ ] **Step 3: Add frontend health/session types and API method**

In `src/api/types.ts`, extend:

```typescript
export interface HealthResponse {
  version: string;
  ready: boolean;
  appSessionId?: string;
}
```

In `src/api/backendClient.ts`, add:

```typescript
async shutdownAppBackend(appSessionId: string): Promise<{ accepted: boolean }> {
  return this.request('/api/app/shutdown', {
    method: 'POST',
    body: JSON.stringify({ appSessionId }),
  });
}
```

- [ ] **Step 4: Add sidecar ownership tests**

Create `frontend/rtx-video-converter/src/lib/tauriBridge.test.ts` with unit tests for pure helpers. Add helper exports from `tauriBridge.ts`:

```typescript
export function createAppSessionId(): string {
  return crypto.randomUUID();
}

export function healthMatchesSession(health: { appSessionId?: string }, appSessionId: string): boolean {
  return health.appSessionId === appSessionId;
}
```

Test:

```typescript
import { describe, expect, it, vi } from 'vitest';
import { createAppSessionId, healthMatchesSession } from './tauriBridge';

describe('tauriBridge app session helpers', () => {
  it('creates a unique app session id', () => {
    const spy = vi.spyOn(crypto, 'randomUUID').mockReturnValue('session-abc');

    expect(createAppSessionId()).toBe('session-abc');

    spy.mockRestore();
  });

  it('requires health to match the sidecar session', () => {
    expect(healthMatchesSession({ appSessionId: 'session-abc' }, 'session-abc')).toBe(true);
    expect(healthMatchesSession({ appSessionId: 'old-session' }, 'session-abc')).toBe(false);
    expect(healthMatchesSession({}, 'session-abc')).toBe(false);
  });
});
```

- [ ] **Step 5: Run helper tests and verify they fail before implementation**

Run:

```powershell
npm test -- tauriBridge.test.ts
```

Expected: FAIL because helper exports do not exist.

- [ ] **Step 6: Implement sidecar session launch**

In `src/lib/tauriBridge.ts`, store the session:

```typescript
let backendChild: SidecarChild | null = null;
let backendSessionId: string | null = null;

export function currentBackendSessionId(): string | null {
  return backendSessionId;
}
```

Update `startBackendSidecar()` so Tauri launches:

```typescript
backendSessionId = createAppSessionId();
const command = Command.sidecar('binaries/vsr_backend', [
  '--port',
  String(port),
  '--app-session-id',
  backendSessionId,
]);
```

If spawn fails, reset both `backendChild` and `backendSessionId`.

- [ ] **Step 7: Verify ownership during backend status refresh**

In `src/hooks/useBackendStatus.ts`, after `waitForHealth()`:

```typescript
const sessionId = currentBackendSessionId();
if (sidecar.runtime === 'tauri' && sessionId !== null && !healthMatchesSession(nextHealth, sessionId)) {
  throw new Error('Backend session does not belong to this app instance.');
}
```

Import `currentBackendSessionId` and `healthMatchesSession`.

- [ ] **Step 8: Stop app-owned backend on normal cleanup**

In `stopBackendSidecar()`, call `backendClient.shutdownAppBackend(sessionId)` before `child.kill()`. If shutdown fails, still call `child.kill()` for the spawned process.

Use a dynamic import to avoid a top-level cycle:

```typescript
const { backendClient } = await import('../api/backendClient');
```

- [ ] **Step 9: Run frontend tests**

Run:

```powershell
npm test
npm run build
```

Expected: all frontend tests and production build pass.

- [ ] **Step 10: Run packaged app lifecycle smoke test**

Build and run:

```powershell
npm run tauri:build
```

Launch `src-tauri/target/release/rtx-video-converter.exe`, confirm one `vsr_backend.exe`, close the window normally, confirm no `vsr_backend.exe` remains.

Expected: normal close cleans the sidecar. Force-kill can still leave an OS process, but the next app launch must reject stale health because `appSessionId` mismatches.

- [ ] **Step 11: Commit**

```powershell
git add frontend/rtx-video-converter/src/api/types.ts frontend/rtx-video-converter/src/api/backendClient.ts frontend/rtx-video-converter/src/api/backendClient.test.ts frontend/rtx-video-converter/src/lib/tauriBridge.ts frontend/rtx-video-converter/src/lib/tauriBridge.test.ts frontend/rtx-video-converter/src/hooks/useBackendStatus.ts
git commit -m "fix: verify packaged app backend ownership"
```

---

### Task 3: Single-Instance or Explicit Multi-Instance Blocking

**Files:**
- Modify: `frontend/rtx-video-converter/src-tauri/Cargo.toml`
- Modify: `frontend/rtx-video-converter/src-tauri/src/lib.rs`
- Modify: `frontend/rtx-video-converter/README.md`

- [ ] **Step 1: Add Tauri single-instance plugin dependency**

In `src-tauri/Cargo.toml`, add:

```toml
tauri-plugin-single-instance = "2"
```

- [ ] **Step 2: Wire the plugin**

In `src-tauri/src/lib.rs`, initialize before `.run()`:

```rust
.plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| {
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.unminimize();
        let _ = window.set_focus();
    }
}))
```

Also import:

```rust
use tauri::Manager;
```

- [ ] **Step 3: Build to verify plugin wiring**

Run:

```powershell
npm run tauri:build
```

Expected: Rust/Tauri release build succeeds.

- [ ] **Step 4: Manual multi-instance smoke test**

Run the built app twice:

```powershell
Start-Process .\src-tauri\target\release\rtx-video-converter.exe
Start-Sleep -Seconds 3
Start-Process .\src-tauri\target\release\rtx-video-converter.exe
Start-Sleep -Seconds 5
Get-Process rtx-video-converter,vsr_backend
```

Expected: one `rtx-video-converter.exe` process and one `vsr_backend.exe` process remain. The existing window should be focused.

- [ ] **Step 5: Document app single-instance behavior**

In `frontend/rtx-video-converter/README.md`, add a short note under Tauri Development:

```markdown
The packaged app is single-instance. Starting the executable again focuses the existing window instead of launching a second sidecar backend.
```

- [ ] **Step 6: Commit**

```powershell
git add frontend/rtx-video-converter/src-tauri/Cargo.toml frontend/rtx-video-converter/src-tauri/Cargo.lock frontend/rtx-video-converter/src-tauri/src/lib.rs frontend/rtx-video-converter/README.md
git commit -m "fix: enforce single app instance"
```

---

### Task 4: Capability-Based Start Gating

**Files:**
- Create: `frontend/rtx-video-converter/src/lib/capabilities.ts`
- Create: `frontend/rtx-video-converter/src/lib/capabilities.test.ts`
- Modify: `frontend/rtx-video-converter/src/App.tsx`
- Modify: `frontend/rtx-video-converter/src/components/StatusFooter.tsx`

- [ ] **Step 1: Write failing capability helper tests**

Create `src/lib/capabilities.test.ts`:

```typescript
import { describe, expect, it } from 'vitest';
import type { CapabilityResponse } from '../api/types';
import { conversionCapabilityProblem } from './capabilities';
import { defaultSettings } from './jobRequest';

const allAvailable: CapabilityResponse = {
  d3d11Available: true,
  rtxSdkFound: true,
  vsrAvailable: true,
  truehdrAvailable: true,
  nvencH264Available: true,
  nvencHevcMain10Available: true,
  messages: [],
};

describe('conversionCapabilityProblem', () => {
  it('allows default VSR plus HDR when all required capabilities exist', () => {
    expect(conversionCapabilityProblem(allAvailable, defaultSettings)).toBeNull();
  });

  it('blocks HDR modes when TrueHDR is unavailable', () => {
    expect(conversionCapabilityProblem({ ...allAvailable, truehdrAvailable: false }, defaultSettings)).toBe('HDR is unavailable on this system.');
  });

  it('blocks VSR-only H264 when H264 NVENC is unavailable', () => {
    expect(
      conversionCapabilityProblem(
        { ...allAvailable, nvencH264Available: false },
        { ...defaultSettings, mode: 'vsr', codec: 'h264' },
      ),
    ).toBe('NVENC H.264 is unavailable on this system.');
  });

  it('blocks HDR output when HEVC Main10 NVENC is unavailable', () => {
    expect(
      conversionCapabilityProblem(
        { ...allAvailable, nvencHevcMain10Available: false },
        { ...defaultSettings, mode: 'hdr', codec: 'hevc' },
      ),
    ).toBe('NVENC HEVC Main10 is unavailable on this system.');
  });

  it('blocks when capabilities are not loaded yet', () => {
    expect(conversionCapabilityProblem(null, defaultSettings)).toBe('Backend capabilities are not loaded yet.');
  });
});
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
npm test -- capabilities.test.ts
```

Expected: FAIL because `capabilities.ts` does not exist.

- [ ] **Step 3: Implement capability helper**

Create `src/lib/capabilities.ts`:

```typescript
import type { CapabilityResponse } from '../api/types';
import type { ConversionSettings } from '../types';

export function conversionCapabilityProblem(capabilities: CapabilityResponse | null, settings: ConversionSettings): string | null {
  if (capabilities === null) {
    return 'Backend capabilities are not loaded yet.';
  }

  const usesVsr = settings.mode === 'vsr' || settings.mode === 'both';
  const usesHdr = settings.mode === 'hdr' || settings.mode === 'both';
  const effectiveCodec = usesHdr ? 'hevc' : settings.codec;

  if (usesVsr && !capabilities.vsrAvailable) {
    return 'VSR is unavailable on this system.';
  }
  if (usesHdr && !capabilities.truehdrAvailable) {
    return 'HDR is unavailable on this system.';
  }
  if (effectiveCodec === 'h264' && !capabilities.nvencH264Available) {
    return 'NVENC H.264 is unavailable on this system.';
  }
  if (effectiveCodec === 'hevc' && !capabilities.nvencHevcMain10Available) {
    return 'NVENC HEVC Main10 is unavailable on this system.';
  }

  return null;
}
```

- [ ] **Step 4: Gate Start in `App.tsx`**

In `App.tsx`, compute:

```typescript
const capabilityProblem = conversionCapabilityProblem(backend.capabilities, settings);
const canStart =
  backend.status !== 'offline' &&
  capabilityProblem === null &&
  !loadingInput &&
  selectedInput.path.length > 0 &&
  resolvedOutputDirectory.length > 0 &&
  job.activeJob === null;
```

Pass `capabilityProblem` to `StatusFooter`.

- [ ] **Step 5: Surface the disabled reason in footer**

In `StatusFooter.tsx`, add prop:

```typescript
disabledReason: string | null;
```

When no active job and `disabledReason !== null`, render it near the Start button in compact text:

```tsx
{!active && disabledReason && <span className="text-xs text-[#b45309]">{disabledReason}</span>}
```

- [ ] **Step 6: Run frontend tests and build**

Run:

```powershell
npm test
npm run build
```

Expected: all tests and build pass.

- [ ] **Step 7: Commit**

```powershell
git add frontend/rtx-video-converter/src/lib/capabilities.ts frontend/rtx-video-converter/src/lib/capabilities.test.ts frontend/rtx-video-converter/src/App.tsx frontend/rtx-video-converter/src/components/StatusFooter.tsx
git commit -m "fix: block unsupported app conversions before submit"
```

---

### Task 5: Input Path Probe Consistency and Immediate Errors

**Files:**
- Modify: `frontend/rtx-video-converter/src/App.tsx`
- Modify: `frontend/rtx-video-converter/src/components/ErrorDetails.tsx`
- Create: `frontend/rtx-video-converter/src/lib/inputSelection.ts`
- Create: `frontend/rtx-video-converter/src/lib/inputSelection.test.ts`

- [ ] **Step 1: Write failing helper tests for manual path behavior**

Create `src/lib/inputSelection.test.ts`:

```typescript
import { describe, expect, it } from 'vitest';
import { nextOutputDirectoryForInput } from './inputSelection';

describe('nextOutputDirectoryForInput', () => {
  it('uses the new input directory when the output directory is empty', () => {
    expect(nextOutputDirectoryForInput('D:\\Videos\\clip.mp4', '')).toBe('D:\\Videos');
  });

  it('preserves a user-selected output directory', () => {
    expect(nextOutputDirectoryForInput('D:\\Videos\\clip.mp4', 'E:\\Exports')).toBe('E:\\Exports');
  });
});
```

- [ ] **Step 2: Run test and verify it fails**

Run:

```powershell
npm test -- inputSelection.test.ts
```

Expected: FAIL because `inputSelection.ts` does not exist.

- [ ] **Step 3: Implement input selection helper**

Create `src/lib/inputSelection.ts`:

```typescript
import { directoryName } from './jobRequest';

export function nextOutputDirectoryForInput(inputPath: string, currentOutputDirectory: string): string {
  return currentOutputDirectory.length === 0 ? directoryName(inputPath) : currentOutputDirectory;
}
```

- [ ] **Step 4: Make manual path changes re-probe on commit**

In `InputPanel.tsx`, prefer probing on blur/Enter rather than every keystroke. Add a prop:

```typescript
onInputPathCommit: (path: string) => void;
```

Wire input:

```tsx
onBlur={(event) => onInputPathCommit(event.target.value)}
onKeyDown={(event) => {
  if (event.key === 'Enter') {
    onInputPathCommit(event.currentTarget.value);
  }
}}
```

In `App.tsx`, implement:

```typescript
const [inputError, setInputError] = useState<ApiError | null>(null);

const loadInput = async (path: string) => {
  if (!path) {
    return;
  }

  setLoadingInput(true);
  setInputError(null);
  setSelectedInput({ path, metadata: null });
  setOutputDirectory((current) => nextOutputDirectoryForInput(path, current));

  try {
    const metadata = await backendClient.probeMedia(path);
    setSelectedInput({ path, metadata });
  } catch (error) {
    setSelectedInput({ path: '', metadata: null });
    setInputError(
      error instanceof ApiError
        ? error
        : new ApiError({
            code: 'media_probe_failed',
            message: 'Media probe failed.',
            details: String(error),
            status: 0,
          }),
    );
  } finally {
    setLoadingInput(false);
  }
};
```

Manual `onInputPathChange` may keep typing state, but committed path must flow through `loadInput`.

- [ ] **Step 5: Display input probe errors**

Render both job error and input error:

```tsx
<ErrorDetails error={inputError ?? job.error} />
```

Clear `inputError` when a valid input probe succeeds.

- [ ] **Step 6: Run frontend tests and build**

Run:

```powershell
npm test
npm run build
```

Expected: all tests and build pass.

- [ ] **Step 7: Manual packaged app smoke test**

Run the packaged app, type a nonexistent path, press Enter.

Expected: input clears or remains non-startable, error details show the probe failure, Start remains disabled.

- [ ] **Step 8: Commit**

```powershell
git add frontend/rtx-video-converter/src/App.tsx frontend/rtx-video-converter/src/components/InputPanel.tsx frontend/rtx-video-converter/src/components/ErrorDetails.tsx frontend/rtx-video-converter/src/lib/inputSelection.ts frontend/rtx-video-converter/src/lib/inputSelection.test.ts
git commit -m "fix: keep app input probing consistent"
```

---

### Task 6: Safe FFmpeg Output Writes

**Files:**
- Modify: `backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp`
- Modify: `backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.h`
- Modify: `backend/tests/unit/ffmpeg_transcode_pipeline_tests.cpp`

- [ ] **Step 1: Add failing tests for temporary output paths**

In `backend/tests/unit/ffmpeg_transcode_pipeline_tests.cpp`, add helper tests:

```cpp
TEST(FfmpegTranscodePipelineOutput, createsTemporaryPathBesideFinalOutput) {
    const auto path = ffmpeg_temporary_output_path("D:\\Videos\\clip_RTX.mp4");

    EXPECT_EQ(path.parent_path().string(), std::filesystem::path("D:\\Videos").string());
    EXPECT_NE(path.filename().string().find("clip_RTX.mp4."), std::string::npos);
    EXPECT_EQ(path.extension(), ".tmp");
}

TEST(FfmpegTranscodePipelineOutput, rejectsMissingOutputDirectory) {
    const auto result = ffmpeg_validate_output_target("Z:\\definitely-missing-vsr-dir\\clip_RTX.mp4");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "output_directory_missing");
}
```

- [ ] **Step 2: Run the backend test and verify it fails**

Run:

```powershell
cmake --build build/backend-audit --target vsr_backend_tests --config Debug
ctest --test-dir build/backend-audit --output-on-failure -C Debug -R FfmpegTranscodePipelineOutput
```

Expected: FAIL because helper functions are not defined.

- [ ] **Step 3: Expose safe output helper declarations**

In `backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.h`, add:

```cpp
std::filesystem::path ffmpeg_temporary_output_path(const std::filesystem::path& final_output);
Result<void> ffmpeg_validate_output_target(const std::filesystem::path& final_output);
Result<void> ffmpeg_replace_output_file(const std::filesystem::path& temporary_output, const std::filesystem::path& final_output);
```

Include `<filesystem>`.

- [ ] **Step 4: Implement safe output helpers**

In `ffmpeg_transcode_pipeline.cpp`, implement:

```cpp
std::filesystem::path ffmpeg_temporary_output_path(const std::filesystem::path& final_output) {
    const auto filename = final_output.filename().string() + "." +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    return final_output.parent_path() / filename;
}

Result<void> ffmpeg_validate_output_target(const std::filesystem::path& final_output) {
    std::error_code error;
    const auto directory = final_output.parent_path();
    if (directory.empty() || !std::filesystem::exists(directory, error)) {
        return Result<void>::Fail({"output_directory_missing", "Output directory does not exist.", directory.string()});
    }
    if (error) {
        return Result<void>::Fail({"output_directory_access_failed", "Output directory could not be checked.", error.message()});
    }
    if (std::filesystem::exists(final_output, error)) {
        return Result<void>::Fail({"output_file_exists", "Output file already exists.", final_output.string()});
    }
    return Result<void>::Ok();
}

Result<void> ffmpeg_replace_output_file(const std::filesystem::path& temporary_output, const std::filesystem::path& final_output) {
    std::error_code error;
    std::filesystem::rename(temporary_output, final_output, error);
    if (error) {
        return Result<void>::Fail({"output_replace_failed", "Temporary output could not be moved to the final path.", error.message()});
    }
    return Result<void>::Ok();
}
```

- [ ] **Step 5: Change the pipeline to write to temporary output**

At the beginning of `FfmpegTranscodePipeline::run`, validate:

```cpp
const auto output_valid = ffmpeg_validate_output_target(request.output_path);
if (!output_valid.ok()) {
    return Result<void>::Fail(output_valid.error());
}
const auto temporary_output = ffmpeg_temporary_output_path(request.output_path);
```

Use `temporary_output.string().c_str()` for `avformat_alloc_output_context2()` and `avio_open()`.

After `av_write_trailer()` and before returning success:

```cpp
const auto replaced = ffmpeg_replace_output_file(temporary_output, request.output_path);
if (!replaced.ok()) {
    return Result<void>::Fail(replaced.error());
}
```

On any error return after the temporary path is created, remove the temporary file with `std::filesystem::remove(temporary_output, ignored)`. Use a small RAII cleanup guard in the function body.

- [ ] **Step 6: Run backend tests**

Run:

```powershell
cmake --build build/backend-audit --target vsr_backend_tests vsr_backend --config Debug
ctest --test-dir build/backend-audit --output-on-failure -C Debug
```

Expected: all backend tests pass.

- [ ] **Step 7: Hardware smoke check if available**

If hardware backend and a sample video are available, run:

```powershell
backend/tests/scripts/hardware_smoke.ps1 -BackendExe build/backend-hw/Release/vsr_backend.exe -InputPath C:\path\to\sample.mp4 -OutputPath C:\path\to\sample_RTX.mp4
```

Expected: no `.tmp` remains after success; output file appears only after success.

- [ ] **Step 8: Commit**

```powershell
git add backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp backend/src/video/ffmpeg/ffmpeg_transcode_pipeline.h backend/tests/unit/ffmpeg_transcode_pipeline_tests.cpp
git commit -m "fix: write ffmpeg output safely"
```

---

### Task 7: Packaging Must Prepare the Current Sidecar

**Files:**
- Modify: `frontend/rtx-video-converter/package.json`
- Modify: `frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1`
- Modify: `frontend/rtx-video-converter/README.md`

- [ ] **Step 1: Update package scripts**

Change `package.json` scripts to:

```json
{
  "sidecar:prepare": "powershell -ExecutionPolicy Bypass -File scripts/prepare-tauri-sidecar.ps1",
  "tauri:build": "npm run sidecar:prepare -- -BackendExe ..\\..\\build\\backend-hw\\Release\\vsr_backend.exe && tauri build",
  "tauri:build:portable": "npm run tauri:build && npm run portable:package"
}
```

If the project standardizes on a different release build directory, use that exact path consistently in this step and the README.

- [ ] **Step 2: Make prepare script fail on missing runtime DLLs for hardware builds**

In `scripts/prepare-tauri-sidecar.ps1`, after copying DLLs, require these files in `src-tauri/runtime`:

```powershell
$RequiredRuntimeDlls = @(
  "avcodec-62.dll",
  "avformat-62.dll",
  "avutil-60.dll",
  "swresample-6.dll",
  "swscale-9.dll",
  "nvngx_vsr.dll",
  "nvngx_truehdr.dll"
)

foreach ($RequiredRuntimeDll in $RequiredRuntimeDlls) {
  $Candidate = Join-Path $RuntimeDir $RequiredRuntimeDll
  if (-not (Test-Path -LiteralPath $Candidate)) {
    throw "Required runtime DLL missing after sidecar preparation: $RequiredRuntimeDll"
  }
}
```

- [ ] **Step 3: Run packaging command and verify**

Run:

```powershell
npm run tauri:build
npm run portable:package
```

Expected: commands succeed only when the current hardware backend and runtime DLLs are present.

- [ ] **Step 4: Verify portable contents**

Run:

```powershell
Get-ChildItem src-tauri/target/release/bundle/portable -Recurse -File | Where-Object { $_.Name -in @("rtx-video-converter.exe","vsr_backend.exe","nvngx_vsr.dll","nvngx_truehdr.dll","avcodec-62.dll") } | Select-Object Name,Length
```

Expected: app exe, backend exe, FFmpeg DLLs, and RTX DLLs are present in the portable directory.

- [ ] **Step 5: Update README**

Update `frontend/rtx-video-converter/README.md` packaging section to state:

```markdown
Before packaging, build the hardware backend in `build/backend-hw/Release/vsr_backend.exe`. `npm run tauri:build` prepares that backend as the Tauri sidecar and fails if required runtime DLLs are missing.
```

- [ ] **Step 6: Commit**

```powershell
git add frontend/rtx-video-converter/package.json frontend/rtx-video-converter/scripts/prepare-tauri-sidecar.ps1 frontend/rtx-video-converter/README.md
git commit -m "fix: package current app sidecar"
```

---

### Task 8: Final App Verification

**Files:**
- No production file edits expected.

- [ ] **Step 1: Run all frontend tests and build**

Run:

```powershell
cd frontend/rtx-video-converter
npm test
npm run build
```

Expected: all Vitest tests pass and Vite production build succeeds.

- [ ] **Step 2: Run all backend tests**

Run:

```powershell
cd C:\Users\21826\Documents\VSR
cmake --build build/backend-audit --target vsr_backend_tests vsr_backend --config Debug
ctest --test-dir build/backend-audit --output-on-failure -C Debug
```

Expected: all GTest/CTest tests pass.

- [ ] **Step 3: Build packaged app**

Run:

```powershell
cd frontend/rtx-video-converter
npm run tauri:build
npm run portable:package
```

Expected: installer and portable build are produced.

- [ ] **Step 4: Verify app startup owns backend**

Run:

```powershell
$AppPath = "C:\Users\21826\Documents\VSR\frontend\rtx-video-converter\src-tauri\target\release\rtx-video-converter.exe"
Get-Process rtx-video-converter,vsr_backend -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
$app = Start-Process -FilePath $AppPath -PassThru
Start-Sleep -Seconds 8
Get-Process rtx-video-converter,vsr_backend
Invoke-RestMethod -Uri "http://127.0.0.1:49321/api/health"
```

Expected: one app process, one backend process, health includes the app session id expected by this app.

- [ ] **Step 5: Verify stale backend rejection**

Run an old/stale backend without `--app-session-id`, then start the packaged app:

```powershell
Start-Process -FilePath "C:\Users\21826\Documents\VSR\frontend\rtx-video-converter\src-tauri\target\release\vsr_backend.exe" -ArgumentList "--port","49321"
Start-Sleep -Seconds 2
Start-Process -FilePath $AppPath
Start-Sleep -Seconds 8
```

Expected: app does not mark backend ready from the stale process; it shows a backend ownership/session error instead of using the stale backend.

- [ ] **Step 6: Verify single-instance behavior**

Run:

```powershell
Start-Process -FilePath $AppPath
Start-Sleep -Seconds 3
Start-Process -FilePath $AppPath
Start-Sleep -Seconds 5
Get-Process rtx-video-converter,vsr_backend
```

Expected: one app process and one backend process.

- [ ] **Step 7: Verify normal close cleanup**

Run:

```powershell
Get-Process rtx-video-converter | ForEach-Object { $_.CloseMainWindow() }
Start-Sleep -Seconds 5
Get-Process rtx-video-converter,vsr_backend -ErrorAction SilentlyContinue
```

Expected: no app or backend process remains.

- [ ] **Step 8: Verify unsupported capabilities block Start**

Temporarily remove or rename one runtime DLL in a copied test build directory, launch the app, and inspect the UI.

Expected: capability warning is shown, Start is disabled with a reason, and no job POST is sent.

- [ ] **Step 9: Verify failed probe blocks Start**

Type a missing file path into the input field and press Enter.

Expected: immediate error is shown and Start remains disabled.

- [ ] **Step 10: Verify safe output**

Cancel a long-running hardware transcode.

Expected: final output path is not replaced by a partial file; temporary output is removed after cancellation.

---

## Self-Review

- Spec coverage: Every app-impacting bug from the scope maps to at least one task. Sidecar ownership is Tasks 1-2, multi-instance is Task 3, capability gating is Task 4, path/probe behavior is Task 5, safe output is Task 6, packaging freshness is Task 7, and full app verification is Task 8.
- Placeholder scan: No `TBD`, `TODO`, or vague "add handling" steps remain. Steps include exact files, commands, and expected outcomes.
- Type consistency: `appSessionId` is used consistently in backend JSON, frontend `HealthResponse`, `shutdownAppBackend`, and ownership helpers.
