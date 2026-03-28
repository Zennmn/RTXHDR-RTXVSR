# Build On Windows

## Confirmed Tool Locations In This Environment

- Flutter:
  - `C:\Users\21826\develop\flutter\bin\flutter.bat`
- CMake:
  - `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- FFmpeg:
  - `C:\Program Files (x86)\ffmpeg-N-122395-g48c9c38684-win64-gpl-shared`
- RTX Video SDK:
  - workspace `RTX_Video_SDK_v1.1.0`
- Video Codec Interface:
  - workspace `Video_Codec_Interface_13.0.37`

## Environment Variables

Optional overrides:

- `FLUTTER_ROOT`
- `CMAKE_EXE`
- `FFMPEG_ROOT`
- `NV_RTX_VIDEO_SDK`
- `NV_VIDEO_CODEC_INTERFACE`
- `RTXHDR_NATIVE_DLL`

## Native Configure

```powershell
pwsh ./app/scripts/configure_native.ps1
```

This script:

- detects Flutter, CMake, FFmpeg, CUDA, and NVIDIA SDK paths
- configures `app/native`
- generates a Visual Studio x64 build tree

## Native Build

```powershell
pwsh ./app/scripts/configure_native.ps1 -Build
```

Expected output DLL:

- `app/native/out/install/bin/rtx_native.dll`

## Flutter Run

```powershell
pwsh ./app/scripts/run_flutter_windows.ps1
```

The script:

- builds the native DLL if needed
- exports `RTXHDR_NATIVE_DLL`
- augments the process `PATH` with FFmpeg and NVIDIA runtime directories
- launches the Flutter Windows app

## Notes

- The first implementation assumes NVIDIA RTX hardware.
- If FFmpeg import libraries are installed in a non-default location, point
  `FFMPEG_ROOT` to the root directory that contains `include`, `lib`, and
  `bin`.
- If `nvngx_truehdr.dll` or `nvngx_vsr.dll` cannot be found at runtime, ensure
  the SDK `bin\Windows\x64\rel` folder is reachable through `PATH`.

