# Project Context

RTXHDR-RTXVSR is a Windows-only desktop app for complete video-file transcoding with NVIDIA RTX Video VSR super resolution, RTX Video HDR/TrueHDR, FFmpeg, D3D11, and NVENC.

NVENC output supports H.264, HEVC/H.265, and AV1. HDR output uses HEVC Main10 or AV1 10-bit with BT.2020/PQ signaling; HEVC and AV1 can use driver-managed Split Frame Encoding on multi-NVENC GPUs.

## Current Status

- The active repository entry branch is `main`.
- The backend lives under `backend/` and is the source of truth for capabilities, media probing, job validation, progress, cancellation, and transcoding.
- The production frontend lives under `frontend/rtx-video-converter-winui/` and uses WinUI 3 on .NET 8.
- Testable presentation and backend-client logic lives in `frontend/rtx-video-converter-winui-core/`.
- WinUI launches `vsr_backend.exe` as a local sidecar and authenticates every request with a per-run session id.
- Release builds hide the extra console window for both installer and portable app launches.

Generated build outputs, SDK folders, FFmpeg binaries, sample videos, and packaged release artifacts stay local and are ignored by git.

## Repository Layout

- `backend/`: C++20 local sidecar backend.
- `backend/src/`: backend source code.
- `backend/tests/`: backend unit tests and hardware smoke script.
- `backend/cmake/`: CMake helper modules.
- `frontend/`: frontend workspace.
- `frontend/rtx-video-converter-winui/`: production WinUI 3 application.
- `frontend/rtx-video-converter-winui-core/`: platform-independent models, services and view models.
- `frontend/rtx-video-converter-winui-core-tests/`: xUnit regression tests.
- `docs/`: planning and implementation notes.
- `FRONTEND_INTEGRATION.md`: HTTP API contract for frontend/backend integration.
- `README.md`: repository overview and build entry points.

## Backend API

The backend listens only on `127.0.0.1` and exposes:

- `GET /api/health`
- `GET /api/capabilities`
- `POST /api/media/probe`
- `POST /api/jobs`
- `GET /api/jobs/{id}`
- `POST /api/jobs/{id}/cancel`

See `FRONTEND_INTEGRATION.md` for request and response examples.

## Build Entry Points

Unit-test backend build:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

Hardware backend build:

```powershell
cmake -S backend -B build\backend-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT="C:\ffmpeg"
cmake --build build\backend-hw --target vsr_backend --config Release
```

WinUI validation:

```powershell
.\build-winui.ps1
```

WinUI publishing with a hardware backend:

```powershell
.\build-winui.ps1 -Publish -BackendExe .\build\backend-hw\Release\vsr_backend.exe
```
