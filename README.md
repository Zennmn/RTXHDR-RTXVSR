# RTXHDR-RTXVSR

Windows-only RTX video conversion app project.

## Project Layout

- `backend/`: native C++ local sidecar backend for RTX VSR, RTX Video HDR, FFmpeg, and NVENC.
- `frontend/`: Windows UI workspace. The current React/Vite UI lives in `frontend/rtx-video-converter/`.
- `FRONTEND_INTEGRATION.md`: API contract for connecting the UI to the backend.
- `PROJECT_CONTEXT.md`: current project status and repository overview.

## Backend Quick Start

From the repository root:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

The root CMake file also delegates to `backend/`, so `cmake -S . -B build` remains available for workspace-level builds.

For project status, read `PROJECT_CONTEXT.md`. For frontend/backend API details, read `FRONTEND_INTEGRATION.md`.
