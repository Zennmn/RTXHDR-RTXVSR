# Frontend Integration Guide

This is the Windows local sidecar API for the RTX Video Converter app.

## Backend Process

Start `vsr_backend.exe --port 49321` as a local sidecar process. The backend listens only on `127.0.0.1`.

## Health

`GET http://127.0.0.1:49321/api/health`

Response:

```json
{ "version": "0.1.0", "ready": true }
```

## Capabilities

`GET /api/capabilities`

Use this endpoint before enabling the main convert button. Show missing requirements from `messages`.

Response:

```json
{
  "d3d11Available": true,
  "rtxSdkFound": true,
  "vsrAvailable": true,
  "truehdrAvailable": true,
  "nvencH264Available": true,
  "nvencHevcMain10Available": true,
  "messages": []
}
```

## Media Probe

`POST /api/media/probe`

Use this endpoint after the user chooses an input file. It validates that the path points to a real file and returns the metadata used by the UI input panel.

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
  "sizeBytes": 123456789,
  "resolution": "1920x1080",
  "duration": "00:01:30",
  "codec": "h264 8-bit",
  "warnings": []
}
```

When the backend is built without FFmpeg support, file validation still runs, but detailed metadata may be empty and `warnings` explains that FFmpeg metadata probing is unavailable.

## Create Job

`POST /api/jobs`

Request:

```json
{
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "processing": {
    "vsr": { "enabled": true, "quality": 3, "scale": 2.0 },
    "hdr": { "enabled": true, "contrast": 100, "saturation": 100, "middleGray": 44, "maxLuminance": 1000 }
  },
  "output": { "container": "mp4", "videoCodec": "hevc", "audioMode": "copy", "subtitleMode": "copy-compatible" }
}
```

Response:

```json
{ "id": "00000000000000000000000001" }
```

## Poll Job

Poll `GET /api/jobs/{id}` every 500 ms while state is `queued`, `running`, or `canceling`.

Terminal states are `succeeded`, `failed`, and `canceled`.

Running response:

```json
{
  "id": "00000000000000000000000001",
  "state": "running",
  "stage": "processing_rtx",
  "progress": 0.42,
  "framesDone": 420,
  "framesTotal": 1000,
  "fps": 58.6,
  "etaSeconds": 10,
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "warnings": []
}
```

Failed or canceled response:

```json
{
  "id": "00000000000000000000000001",
  "state": "failed",
  "stage": "processing_rtx",
  "progress": 0.42,
  "framesDone": 420,
  "framesTotal": 1000,
  "fps": 0.0,
  "etaSeconds": 0,
  "inputPath": "C:\\Videos\\input.mp4",
  "outputPath": "C:\\Videos\\input.rtx.mp4",
  "warnings": [],
  "error": {
    "code": "transcode_failed",
    "message": "Transcode failed.",
    "details": "FFmpeg or RTX SDK diagnostic text"
  }
}
```

For terminal failures and cancellations, `stage` remains the last reported work stage.

## Cancel Job

`POST /api/jobs/{id}/cancel`

Response:

```json
{ "accepted": true }
```

## HTTP Statuses

- `GET /api/health`: `200`.
- `GET /api/capabilities`: `200`.
- `POST /api/media/probe`: `200` with media metadata on success; `400` for invalid JSON, missing input path, missing file, non-file path, or probe errors.
- `POST /api/jobs`: `202` with `{ "id": "..." }` on success; `400` for invalid JSON, invalid paths, invalid parameter ranges, unsupported output options, or missing processing options; `503` when the server is shutting down.
- `GET /api/jobs/{id}`: `200` with a job snapshot; `404` when the job id is unknown.
- `POST /api/jobs/{id}/cancel`: `200` with `{ "accepted": true }` on success; `404` when the job id is unknown; `409` when the job already reached a terminal state; `400` for other cancel errors.

Error responses always use:

```json
{
  "error": {
    "code": "invalid_json",
    "message": "Request JSON is invalid.",
    "details": ""
  }
}
```

## UI Parameter Ranges

- VSR quality: integer 1 to 4, default 3.
- VSR scale: 1.0 to 4.0, default 2.0.
- HDR contrast: integer, default 100.
- HDR saturation: integer, default 100.
- HDR middle gray: integer, default 44.
- HDR max luminance: integer nits, default 1000.
- HDR output codec: `hevc`.
- VSR-only default codec: `h264`.
- Audio mode: `copy` or `none`, default `copy`.
- Subtitle mode: `copy-compatible` or `none`, default `copy-compatible`.

## Recommended UI States

- Disable Convert until health is ready and capabilities are loaded.
- Show capability warnings above settings.
- During a job, show stage, percent, frames, fps, and ETA.
- Keep Cancel visible while state is `queued` or `running`.
- On failure, show `error.message` and put `error.details` behind an expandable details row.
