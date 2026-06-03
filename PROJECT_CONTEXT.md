# Project Status

## Summary

RTXHDR-RTXVSR is a Windows-only desktop app project for complete video-file transcoding with NVIDIA RTX Video VSR super resolution, RTX Video HDR/TrueHDR, FFmpeg, D3D11, and NVENC.

## Current Status

- The backend exists and is the stable source of truth for supported capabilities.
- The backend has been reorganized under `backend/`.
- A React/Vite UI project currently exists at `frontend/rtx-video-converter/`.
- The active development branch for UI work is `frontend`.
- The root integration contract is `FRONTEND_INTEGRATION.md`.

## Repository Layout

- `backend/`: C++20 local sidecar backend.
- `backend/src/`: backend source code.
- `backend/tests/`: backend unit tests and hardware smoke script.
- `backend/cmake/`: CMake helper modules.
- `backend/docs/`: backend planning/spec history.
- `frontend/`: frontend workspace.
- `frontend/rtx-video-converter/`: current Vite + React + TypeScript UI project.
- `FRONTEND_INTEGRATION.md`: HTTP API contract for frontend/backend integration.
- `README.md`: repository overview and build entry points.

## Backend Current Scope

The backend is a local Windows sidecar process. It listens on `127.0.0.1` and exposes a small HTTP API:

- `GET /api/health`
- `GET /api/capabilities`
- `POST /api/jobs`
- `GET /api/jobs/{id}`
- `POST /api/jobs/{id}/cancel`

Supported processing:

- VSR enabled/disabled
- VSR quality `1..4`
- VSR scale `1.0..4.0`
- HDR enabled/disabled
- HDR integer parameters: `contrast`, `saturation`, `middleGray`, `maxLuminance`
- MP4 output only
- H.264 or HEVC for VSR-only output
- HEVC Main10 required when HDR is enabled
- Audio mode: `copy` or `none`
- Subtitle mode: `copy-compatible` or `none`

## Backend Features Not Present Yet

The backend does not currently provide:

- GPU model, GPU utilization, VRAM, temperature, Tensor Core status, or driver information
- system HDR display detection
- media-info/probe endpoint before job creation
- thumbnail or preview generation
- job list/history persistence
- log browsing API
- settings API
- multi-GPU scheduling
- batch management API
- MKV/MOV containers
- AV1, ProRes, VP9, or advanced encoder controls
- audio/subtitle transcoding or per-track selection

## Frontend Current Shape

- The current UI project is `frontend/rtx-video-converter/`.
- The frontend stack is Vite + React + TypeScript.
- The UI currently has a WinUI-style converter layout.
- The UI currently contains simulated file information and simulated processing progress.
- The backend API contract for real integration is `FRONTEND_INTEGRATION.md`.

## App Packaging Note

Plain Vite/React running in a browser cannot access real Windows file paths such as `C:\Videos\input.mp4`. For the real app, the UI should eventually run inside a Windows app shell such as Tauri or another native wrapper that can choose files, choose folders, and launch `vsr_backend.exe` as a sidecar process.

## Build and Test Commands

Backend unit-test build from repository root:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

Frontend commands from `frontend/rtx-video-converter/`:

```powershell
npm install
npm run lint
npm run build
npm run dev
```
