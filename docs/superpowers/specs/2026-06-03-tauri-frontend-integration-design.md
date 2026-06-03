# Tauri Frontend Integration Design

## Goal

Build the existing React/Vite WinUI-style frontend into a functional Windows desktop app that integrates with the C++ backend sidecar. The finished frontend should select real Windows files and folders, start the local backend, show real backend capabilities, submit transcode jobs, poll progress, cancel jobs, and display backend errors clearly.

## Confirmed Direction

Use Tauri v2 as the Windows app shell. Keep the React UI as the main renderer and keep backend communication through the documented local HTTP API. The backend is mostly complete but not fully fixed, so the frontend work may add small backend endpoints when the UI cannot reliably complete a real workflow without them.

The preferred backend addition is a media probe endpoint:

```http
POST /api/media/probe
```

Request:

```json
{
  "inputPath": "C:\\Videos\\input.mp4"
}
```

Response:

```json
{
  "path": "C:\\Videos\\input.mp4",
  "name": "input.mp4",
  "sizeBytes": 345001234,
  "resolution": "1920x1080",
  "duration": "00:24:15",
  "codec": "H.264 8-bit"
}
```

This endpoint should reuse the existing FFmpeg probe code under `backend/src/video/ffmpeg/` where possible. If FFmpeg support is disabled in a development build, the endpoint should still return path, name, and size, and mark unavailable media fields as empty strings with a warning.

## Architecture

The application has three layers:

1. Tauri shell: provides native Windows capabilities, bundles and launches `vsr_backend.exe`, and opens file/folder pickers.
2. React frontend: owns UI state, form state, capabilities display, job progress, cancellation state, and error presentation.
3. C++ backend sidecar: remains the source of truth for capabilities, request validation, transcode job execution, progress, cancellation, and media probing.

The React app must not import backend C++ headers. The integration boundary is HTTP plus Tauri commands/plugins.

Tauri v2 implementation details:

- Bundle the backend as a sidecar through `tauri.conf.json` `bundle.externalBin`.
- Use the Tauri dialog plugin for input video and output directory selection.
- Use the Tauri shell plugin or Rust-side command wrapper to spawn `vsr_backend.exe --port 49321`.
- Configure `src-tauri/capabilities/default.json` to allow the required dialog and sidecar permissions.
- Keep browser development mode usable by connecting to a manually started backend on `http://127.0.0.1:49321`.

## Frontend File Boundaries

`frontend/rtx-video-converter/src/api/backendClient.ts`

- Owns all backend HTTP calls.
- Implements `getHealth`, `getCapabilities`, `probeMedia`, `createJob`, `getJob`, and `cancelJob`.
- Parses backend error envelopes and converts network failures into typed frontend errors.
- Uses the backend base URL from config, defaulting to `http://127.0.0.1:49321`.

`frontend/rtx-video-converter/src/lib/jobRequest.ts`

- Converts UI settings into the backend `TranscodeRequest` JSON.
- Maps modes: `vsr`, `hdr`, and `both`.
- Forces `hevc` whenever HDR is enabled.
- Maps audio to `copy` or `none`.
- Maps subtitles to `copy-compatible` or `none`.
- Generates the default output path as `<output directory>\<input stem>_RTX.mp4`.

`frontend/rtx-video-converter/src/lib/tauriBridge.ts`

- Detects whether the app is running inside Tauri.
- Starts the backend sidecar in Tauri mode.
- Opens the input video picker.
- Opens the output directory picker.
- Provides browser-mode fallback behavior without pretending browser mode can access full native paths.

`frontend/rtx-video-converter/src/hooks/useBackendStatus.ts`

- Starts or waits for the backend.
- Polls health until ready or timeout.
- Loads capabilities after health succeeds.
- Exposes `starting`, `ready`, `offline`, and `degraded` UI states.

`frontend/rtx-video-converter/src/hooks/useTranscodeJob.ts`

- Creates one job at a time.
- Polls `GET /api/jobs/{id}` every 500 ms while the state is `queued`, `running`, or `canceling`.
- Stops polling on `succeeded`, `failed`, or `canceled`.
- Calls cancel API and waits for the backend terminal state instead of locally faking cancellation.

`frontend/rtx-video-converter/src/components/`

- Keep the current WinUI styling language.
- Split the current large `App.tsx` into focused UI components:
  - `InputPanel`
  - `SettingsPanel`
  - `StatusFooter`
  - `CapabilityBanner`
  - `ErrorDetails`
- Keep `WinUI.tsx` for small reusable controls such as sliders, segmented controls, and switches.

## Backend File Boundaries

`backend/src/api/http_server.cpp`

- Add `POST /api/media/probe`.
- Return JSON errors through the existing `error_to_json` envelope.

`backend/src/api/json_dto.h` and `backend/src/api/json_dto.cpp`

- Add DTO parsing and serialization for media probe requests and responses.
- Keep naming aligned with the existing camelCase JSON contract.

`backend/src/video/ffmpeg/ffmpeg_probe.h` and `backend/src/video/ffmpeg/ffmpeg_probe.cpp`

- Reuse or extend existing probe functionality to produce UI-facing media metadata.
- Keep FFmpeg-specific logic outside `http_server.cpp`.

`backend/tests/unit/`

- Add route/DTO tests for valid probe requests, missing input path, invalid JSON, and FFmpeg-disabled fallback metadata if feasible in the current build configuration.

## Main User Flow

1. App launches.
2. In Tauri mode, the shell starts `vsr_backend.exe --port 49321`.
3. Frontend polls `/api/health` until the backend is ready.
4. Frontend loads `/api/capabilities`.
5. UI disables unavailable processing combinations based on capabilities.
6. User selects or drops a video file.
7. Frontend calls `/api/media/probe` and fills the input panel with real metadata.
8. User selects output directory or accepts the default.
9. Frontend builds a `TranscodeRequest`.
10. Frontend posts `/api/jobs`.
11. Frontend polls `/api/jobs/{id}` every 500 ms.
12. Footer shows state, stage, progress, frames, FPS, ETA, warnings, and terminal errors.
13. User can cancel while the job is `queued`, `running`, or `canceling`.

## UI State Rules

- Disable "Start Transcode" until health is ready, capabilities are loaded, an input path exists, and an output path can be generated.
- Show capability messages above settings.
- Disable VSR mode when `vsrAvailable` is false.
- Disable HDR mode when `truehdrAvailable` is false.
- Disable H.264 when `nvencH264Available` is false.
- Disable HEVC Main10 when `nvencHevcMain10Available` is false.
- Force HEVC Main10 when HDR is enabled.
- Keep cancel visible while the active job is not terminal.
- Do not show simulated progress after real job submission exists.
- In browser mode, clearly label the app as development mode if native file selection and sidecar launch are unavailable.

## Error Handling

The frontend should distinguish these cases:

- Backend offline or starting: show a calm connection message and retry health briefly.
- Capability missing: show backend `messages` and disable affected controls.
- Backend validation error: show `error.message` and expandable `error.details`.
- Job failure: show terminal job error and preserve last known progress/stage.
- Cancel conflict `409`: refresh the job snapshot because the job may already be terminal.
- Probe failure: keep the selected path visible, clear detailed media fields, and show the backend error.

## Testing And Verification

Frontend verification:

```powershell
cd frontend\rtx-video-converter
npm install
npm run lint
npm run build
```

Frontend unit tests should be added if the project introduces a test runner. At minimum, TypeScript coverage should validate API DTO types and request construction.

Backend verification:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

Manual integration verification:

```powershell
.\build\backend\Debug\vsr_backend.exe --port 49321
cd frontend\rtx-video-converter
npm run dev
```

Then verify health, capabilities, media probe, job creation, polling, cancellation, and terminal error display through the UI.

Windows app verification:

```powershell
cd frontend\rtx-video-converter
npm run tauri dev
npm run tauri build
```

The packaged app must include the backend sidecar and required runtime DLLs for the selected backend build.

## Scope Boundaries

Included:

- Tauri v2 shell setup.
- Sidecar launch wiring.
- Dialog-based file and folder selection.
- Real backend API integration.
- Media probe endpoint if required by the frontend.
- Capability-gated UI controls.
- Real job polling and cancellation.
- Clear error and warning display.
- Removal of AI Studio template leftovers that conflict with the app.

Excluded from this design:

- Batch queue management.
- Persistent job history.
- Settings persistence.
- Log browser UI.
- GPU telemetry.
- Preview thumbnails.
- Advanced encoder controls.
- New containers or codecs beyond the backend contract.

These excluded items should each get a separate spec after the single-job Windows app flow works end to end.
