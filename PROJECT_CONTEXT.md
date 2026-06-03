# Project Context

RTXHDR-RTXVSR is a Windows-only desktop app for complete video-file transcoding with NVIDIA RTX Video VSR super resolution, RTX Video HDR/TrueHDR, FFmpeg, D3D11, and NVENC.

## Current Status

- The active repository entry branch is `main`.
- The backend lives under `backend/` and is the source of truth for capabilities, media probing, job validation, progress, cancellation, and transcoding.
- The frontend lives under `frontend/rtx-video-converter/` and uses React, Vite, TypeScript, and Tauri.
- The frontend is wired to the real backend HTTP API.
- Tauri launches `vsr_backend.exe` as a local sidecar and supports native Windows file/folder dialogs.
- Windows packaging supports both NSIS installer output and a no-install portable zip.
- Release builds hide the extra console window for both installer and portable app launches.

Generated build outputs, SDK folders, FFmpeg binaries, sample videos, and packaged release artifacts stay local and are ignored by git.

## Repository Layout

- `backend/`: C++20 local sidecar backend.
- `backend/src/`: backend source code.
- `backend/tests/`: backend unit tests and hardware smoke script.
- `backend/cmake/`: CMake helper modules.
- `frontend/`: frontend workspace.
- `frontend/rtx-video-converter/`: current Vite + React + TypeScript + Tauri Windows app.
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

Frontend validation:

```powershell
cd frontend\rtx-video-converter
npm install
npm run lint
npm run test
npm run build
```

Windows packaging:

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend-hw\Release\vsr_backend.exe
npm run tauri:build:portable
```
