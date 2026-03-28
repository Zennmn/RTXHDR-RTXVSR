# Troubleshooting

## Flutter Or CMake Not Found

Use the provided scripts first. They detect the confirmed tool paths for this
machine and only fall back to `PATH` when needed.

## NGX Runtime DLL Missing

Symptoms:

- `probe_system` reports NGX unavailable
- job start fails before processing begins

Fix:

- confirm `NV_RTX_VIDEO_SDK` or the workspace SDK points to a valid SDK root
- ensure `bin\Windows\x64\rel` is reachable through the process `PATH`

## FFmpeg Probe Fails

Symptoms:

- media analysis returns open-input or stream-info errors

Fix:

- verify the input path exists and is readable
- confirm `FFMPEG_ROOT` points to the shared build with `include`, `lib`, and
  `bin`
- make sure FFmpeg runtime DLLs are reachable through `PATH`

## Job Cancels But Output File Remains

The pipeline writes to a temporary output first and tries to clean it on
failure or cancellation. If a file remains, it is safe to remove manually.

## HDR Output Looks Wrong

Check:

- output codec is `HEVC Main10` or `AV1 10-bit`
- HDR export was enabled
- the result probe reports `bt2020` + `smpte2084`
- the target player actually recognizes HDR metadata

## Audio Copy Fails For MP4

The pipeline uses a conservative passthrough policy. If the original codec is
not considered safe for MP4, the exporter will switch to AAC.

