# RTX Video Backend

Windows-only local backend for complete-video RTX VSR and RTX Video HDR transcoding.

The backend is intentionally isolated under this folder so the future Windows UI can live under `../frontend`.

## Build Unit-Test Backend

From the repository root:

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

## Build Hardware Backend

Install a Windows FFmpeg developer package with D3D11VA and NVENC support, then set `FFMPEG_ROOT`.
Download/unpack NVIDIA RTX Video SDK 1.1.0 to the repository root as `RTX_Video_SDK_v1.1.0`, or pass `-DVSR_RTX_SDK_ROOT=C:\path\to\RTX_Video_SDK_v1.1.0`.

```powershell
cmake -S backend -B build\backend-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT=C:\ffmpeg
cmake --build build\backend-hw --target vsr_backend --config Release
```

## Run

```powershell
.\build\backend-hw\Release\vsr_backend.exe --port 49321
```

Open `..\FRONTEND_INTEGRATION.md` for API details from this folder, or `FRONTEND_INTEGRATION.md` from the repository root.
