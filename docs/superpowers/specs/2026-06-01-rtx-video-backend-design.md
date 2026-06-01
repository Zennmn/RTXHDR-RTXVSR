# RTX Video Backend Design

Date: 2026-06-01

Status: Approved for implementation planning

## Purpose

Build the Windows backend for a future lightweight desktop app that enhances complete video files with NVIDIA RTX Video Super Resolution and RTX Video HDR. The backend should stay small, local, and front-end friendly: a later Tauri UI can submit jobs, show progress, cancel work, and open output files without knowing the details of FFmpeg, D3D, CUDA, or NGX.

## Current Project Context

The repository currently contains NVIDIA RTX Video SDK 1.1.0 under `RTX_Video_SDK_v1.1.0`. The SDK includes headers, Windows x64/arm64 NGX libraries, TrueHDR/VSR runtime DLLs, and samples for DX11, DX12, CUDA, Vulkan, VSR, TrueHDR, and combined TrueHDR+VSR. The sample `RTX_Video_SDK_v1.1.0/samples/RTX_Video_API/rtx_video_api.h` exposes C-style create/evaluate/shutdown functions for DX11, DX12, CUDA, and Vulkan. The DX11 and DX12 sample implementations already evaluate VSR first and TrueHDR second when both features are enabled.

Official NVIDIA references checked during design:

- `https://developer.nvidia.com/rtx-video-sdk/getting-started`
- `https://developer.nvidia.com/rtx-video-sdk`
- `https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/ffmpeg-with-nvidia-gpu/index.html`

These references confirm that RTX Video SDK supports super resolution, artifact reduction, and SDR-to-HDR tonemapping; that RTX Video SDK 1.1 adds native CUDA support, RTX 50 series support, and 10-bit VSR support; and that FFmpeg can use NVIDIA GPU acceleration through NVDEC/NVENC on Windows.

## Product Constraints

- Platform: Windows 10/11 only.
- Hardware: NVIDIA RTX GPU and compatible NVIDIA driver.
- Backend shape: local sidecar process, not a remote server.
- Network surface: listen only on `127.0.0.1`.
- UI direction: future Tauri app, because it keeps the app lighter than Electron while allowing attractive web-based UI.
- MVP input: complete video files.
- MVP output: MP4 files.
- MVP concurrency: one active transcode job at a time, with a queue-shaped API so later releases can add batch processing.
- MVP media behavior: process the primary video stream; copy audio streams when MP4-compatible; copy subtitle streams when MP4-compatible; report skipped incompatible streams in job warnings.
- MVP processing modes: VSR only, RTX Video HDR only, or VSR plus RTX Video HDR.

## Non-Goals For The First Backend

- No front-end implementation.
- No cloud processing.
- No Electron shell.
- No MKV output.
- No AV1 output unless the installed FFmpeg/NVENC stack exposes it and the user explicitly enables an internal experimental option later.
- No multi-GPU scheduling.
- No scene editor, timeline, preview player, or interactive comparison UI.
- No CPU fallback for RTX processing. If the GPU path is unavailable, the backend returns a clear capability or job error.

## Architecture Decision

Use a single native C++ executable named `vsr_backend.exe`. It runs as a local service for the future Tauri UI and exposes JSON APIs over localhost HTTP. This preserves the low operational overhead of a command-line tool while avoiding brittle stdout parsing and giving the UI stable task, progress, cancel, and capability endpoints.

The backend uses layered modules:

- `api`: localhost HTTP routes and JSON serialization.
- `jobs`: job IDs, state transitions, progress snapshots, cancellation, and single-worker scheduling.
- `video`: transcode orchestration, input probing, progress reporting, and media warnings.
- `video/ffmpeg`: FFmpeg demux/decode/encode/mux integration.
- `video/rtx`: RTX Video SDK integration behind a narrow interface.
- `platform`: Windows path, process, DLL, and GPU capability helpers.

## RTX Integration Strategy

Prefer the DX11 GPU-texture path for the first production pipeline because the app is Windows-only and FFmpeg can operate with D3D11VA hardware frames. The SDK already provides DX11 helpers and sample API functions:

- `rtx_video_api_dx11_create(ID3D11Device*, THDREnable, VSREnable)`
- `rtx_video_api_dx11_evaluate(ID3D11Texture2D*, ID3D11Texture2D*, inputRect, outputRect, VSRSetting*, THDRSetting*)`
- `rtx_video_api_dx11_shutdown()`

This keeps the design aligned with Windows hardware decode/encode surfaces and avoids introducing a required CPU frame round-trip. The backend will still keep the RTX adapter interface independent of DX11 so a future CUDA backend can be added if the pipeline needs CUDA-only filters or tighter integration with CUDA FFmpeg frames.

## Video Pipeline

The job pipeline is:

1. Validate request paths and processing settings.
2. Probe input with FFmpeg.
3. Select output dimensions and video codec settings.
4. Create a D3D11 device used by FFmpeg hardware frames and RTX Video SDK evaluation.
5. Decode the primary video stream with FFmpeg hardware acceleration.
6. Convert or allocate GPU textures in the format required by the RTX Video SDK wrapper.
7. Evaluate VSR, TrueHDR, or VSR followed by TrueHDR.
8. Encode the processed frames with NVENC.
9. Mux processed video plus copied compatible audio/subtitle streams into MP4.
10. Emit progress snapshots and final job result.

For TrueHDR output, the encoder target is HEVC Main10 with HDR10-compatible metadata. For VSR-only SDR output, the encoder target can be H.264 NVENC or HEVC NVENC, with H.264 as the initial default for compatibility.

## Public API

All endpoints return JSON. Error responses include `code`, `message`, and optional `details`.

`GET /api/health`

Returns backend version and readiness.

`GET /api/capabilities`

Returns detected GPU/API support:

- RTX Video SDK runtime found.
- VSR available.
- TrueHDR available.
- NVENC H.264 available.
- NVENC HEVC Main10 available.
- D3D11 device creation status.
- Relevant driver or SDK messages when detection fails.

`POST /api/jobs`

Creates a transcode job. Initial request shape:

```json
{
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "processing": {
    "vsr": { "enabled": true, "quality": 3, "scale": 2.0 },
    "hdr": { "enabled": true, "contrast": 100, "saturation": 100, "middleGray": 44, "maxLuminance": 1000 }
  },
  "output": {
    "container": "mp4",
    "videoCodec": "hevc",
    "audioMode": "copy",
    "subtitleMode": "copy-compatible"
  }
}
```

`GET /api/jobs/{id}`

Returns job state:

```json
{
  "id": "01JZ0000000000000000000000",
  "state": "running",
  "stage": "encoding",
  "progress": 0.42,
  "framesDone": 3021,
  "framesTotal": 7198,
  "fps": 71.4,
  "etaSeconds": 58,
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "warnings": []
}
```

`POST /api/jobs/{id}/cancel`

Requests cancellation. The worker stops at the next safe checkpoint and marks the job `canceled`.

The MVP uses polling from the UI. A WebSocket or server-sent-events stream can be added later without changing the job model.

## Job State Model

Job states:

- `queued`: accepted but not running.
- `running`: worker owns the job.
- `succeeded`: output file was written.
- `failed`: job ended with a structured error.
- `canceling`: cancellation was requested.
- `canceled`: worker stopped without producing a completed output.

Stages:

- `validating`
- `probing`
- `initializing_gpu`
- `decoding`
- `processing_rtx`
- `encoding`
- `muxing`
- `finalizing`

## Error Handling

Errors are explicit and front-end readable. Examples:

- `input_not_found`
- `output_not_writable`
- `rtx_sdk_missing`
- `vsr_unavailable`
- `truehdr_unavailable`
- `nvenc_unavailable`
- `unsupported_input_video`
- `unsupported_mp4_stream`
- `transcode_failed`
- `job_canceled`

The backend logs developer detail to a local log file while returning concise messages to the UI.

## Frontend Integration Requirements

The later UI should be able to build a polished workflow around:

- Capability panel: show available RTX features and missing requirements.
- File picker: submit absolute Windows paths to the backend.
- Settings panel: expose only stable MVP parameters first.
- Job progress: poll `GET /api/jobs/{id}` every 500 ms while running.
- Result actions: open output folder, copy output path, retry with modified settings.
- Failure panel: show `message`, expandable `details`, and logs location.

The backend documentation must include endpoint schemas, example requests, example responses, recommended polling behavior, parameter ranges, and UI copy guidance for common errors.

## Testing Strategy

Tests should not require an RTX GPU unless explicitly marked as hardware integration tests.

Coverage layers:

- Unit tests for request validation, JSON serialization, job state transitions, cancellation, and capability mapping.
- Fake pipeline tests that drive progress and failure paths without FFmpeg or RTX hardware.
- Build-time checks for optional FFmpeg and RTX SDK linkage.
- Manual hardware smoke test on Windows with an RTX GPU and a short video.

The first implementation should keep hardware-specific code behind interfaces so most tests can run on any development machine.

## Extensibility Plan

The MVP keeps the public API and internal types ready for later releases:

- Additional containers: add `mkv` to `output.container`.
- Additional codecs: add `av1` and encoder-specific options under `output.video`.
- Batch queue: raise worker concurrency or run one worker process per job.
- Preview mode: add a frame extraction endpoint using the same RTX adapter.
- UI events: add WebSocket/SSE using the existing job snapshot type.
- Multiple RTX backends: add CUDA or D3D12 implementations behind the `RtxProcessor` interface.

## Acceptance Criteria

- The repository contains a native Windows backend project.
- The backend exposes documented localhost JSON APIs for health, capabilities, job creation, job status, and cancellation.
- The backend accepts a complete input video file and produces an MP4 output file through the native video pipeline.
- The RTX processing layer calls NVIDIA RTX Video SDK VSR and TrueHDR APIs through the local SDK.
- The backend provides structured progress and structured failures suitable for a future Tauri UI.
- The frontend integration document is specific enough for a UI implementation to begin without reading the C++ internals.
