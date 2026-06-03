# RTXHDR-RTXVSR

RTXHDR-RTXVSR is a Windows desktop video conversion app that applies NVIDIA RTX Video VSR super resolution and RTX Video HDR/TrueHDR to complete video files.

The app is built as a local desktop stack:

- A C++20 backend sidecar exposes a localhost HTTP API, probes media, detects RTX/NVENC capabilities, and runs FFmpeg/NVIDIA processing jobs.
- A React/Vite/TypeScript frontend provides the converter UI.
- A Tauri Windows shell launches the backend sidecar, opens native file/folder dialogs, and builds installer or portable packages.

## Current Status

- Backend source, tests, FFmpeg integration, RTX SDK integration, HTTP API, media probing, job polling, and cancellation are implemented.
- Frontend source is wired to the real backend API instead of mocked progress.
- Tauri packaging is configured for Windows installer builds and no-install portable zip builds.
- Release builds hide the extra console window for both installer and portable app launches.

Build outputs, SDK bundles, FFmpeg binaries, sample videos, and packaged `.exe` / `.zip` artifacts are intentionally not committed. They are generated locally from the source in this repository.

## Repository Layout

```text
.
|-- backend/                         C++ local sidecar backend
|   |-- src/                         Backend source code
|   |-- tests/                       Unit tests and hardware smoke script
|   `-- cmake/                       CMake helper modules
|-- frontend/
|   `-- rtx-video-converter/         React/Vite/Tauri Windows frontend
|-- docs/                            Planning and implementation notes
|-- FRONTEND_INTEGRATION.md          Backend HTTP API contract
|-- PROJECT_CONTEXT.md               Project status notes
`-- CMakeLists.txt                   Workspace CMake entry point
```

## Requirements

For unit-test and UI development:

- Windows 10/11
- CMake 3.25 or newer
- A C++20 compiler, normally Visual Studio Build Tools or Visual Studio
- Node.js and npm
- Rust and Cargo for Tauri

For real RTX hardware conversion:

- Supported NVIDIA RTX GPU
- Recent NVIDIA driver with RTX Video support
- FFmpeg Windows developer package with headers, import libraries, D3D11VA, and NVENC support
- NVIDIA RTX Video SDK 1.1.0

The default backend CMake option expects the SDK at:

```text
RTX_Video_SDK_v1.1.0/
```

You can also pass a custom path with `-DVSR_RTX_SDK_ROOT=...`.

## Backend: Unit-Test Build

From the repository root:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

This mode does not require FFmpeg, the RTX SDK, or RTX hardware.

## Backend: Hardware Build

Set `FFMPEG_ROOT` to a Windows FFmpeg developer package, or pass it directly to CMake.

Example:

```powershell
cmake -S backend -B build\backend-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT="C:\ffmpeg"
cmake --build build\backend-hw --target vsr_backend --config Release
```

Run the backend manually:

```powershell
.\build\backend-hw\Release\vsr_backend.exe --port 49321
```

The backend listens only on `127.0.0.1`.

## Frontend Development

From the frontend project:

```powershell
cd frontend\rtx-video-converter
npm install
npm run lint
npm run test
npm run build
```

Browser development mode:

```powershell
npm run dev
```

In browser mode, start the backend manually first. Browser mode can call the backend API, but it cannot open native Windows file/folder dialogs.

## Tauri Development

Build the backend first, then prepare it as a Tauri sidecar.

Debug/unit-test sidecar example:

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend\Debug\vsr_backend.exe
npm run tauri:dev
```

Hardware sidecar example:

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend-hw\Release\vsr_backend.exe
npm run tauri:dev
```

The sidecar preparation script copies `vsr_backend.exe` and adjacent runtime DLLs into the Tauri packaging layout.

## Windows Packaging

Build the Windows installer:

```powershell
cd frontend\rtx-video-converter
npm run tauri:build
```

Build the installer and portable package:

```powershell
cd frontend\rtx-video-converter
npm run tauri:build:portable
```

Expected generated outputs:

```text
frontend/rtx-video-converter/src-tauri/target/release/bundle/nsis/
frontend/rtx-video-converter/src-tauri/target/release/bundle/portable/
```

These generated artifacts are ignored by git.

## Backend API

The frontend talks to the local backend through:

- `GET /api/health`
- `GET /api/capabilities`
- `POST /api/media/probe`
- `POST /api/jobs`
- `GET /api/jobs/{id}`
- `POST /api/jobs/{id}/cancel`

See `FRONTEND_INTEGRATION.md` for request and response examples.

## Notes For Contributors

- Keep generated build outputs out of git.
- Keep large SDK, FFmpeg, video, and packaged binary artifacts local.
- Use the unit-test backend build for source validation when hardware dependencies are unavailable.
- Use the hardware backend build before producing real installer or portable app releases.
