# Architecture

## Module Boundaries

- `core`
  - canonical enums and value types
  - structured status and error propagation
  - logging and validation
- `video_io`
  - FFmpeg runtime discovery
  - media probing
  - hardware codec capability discovery
- `enhance`
  - `IEnhancementBackend`
  - `IEnhancementSession`
  - `NullEnhancementBackend`
  - `NvidiaCudaNgxBackend`
- `pipeline`
  - job manager
  - processing orchestration
  - progress, ETA, cancellation, temporary output handling
- `platform`
  - Windows capability probing
  - stable C ABI for Flutter

## Data Flow

1. Flutter calls `rtx_probe_system` and `rtx_analyze_input`.
2. Flutter creates a job with `rtx_create_job`.
3. `JobManager` validates configuration and starts a worker thread.
4. `ProcessingPipeline` probes the input, chooses output size and backend, then:
   - launches FFmpeg decoder to emit `bgra` raw frames
   - pushes frames into an enhancement session
   - sends processed frames into an FFmpeg encoder/muxer process
5. The job reports progress and events through thread-safe polling APIs.
6. The pipeline validates the final file and renames the temporary output into
   place.

## Thread Model

- Flutter UI thread never blocks on long native work.
- Every job uses one native worker thread.
- Progress and logs are stored behind a mutex and queried through polling.
- Cancellation is a shared atomic flag checked during every frame loop.
- FFmpeg child processes are owned by the worker thread and are terminated when
  the job is cancelled or fails.

## Memory Ownership

- The native library owns all job records and lifetime-managed backend sessions.
- The C ABI exposes only opaque job ids and POD structs.
- FFmpeg probing objects are scoped with RAII.
- Pipe buffers are allocated per job and reused for the whole transcode loop.
- The NVIDIA sample API backend currently copies host-memory frames into CUDA
  resources internally. That boundary is isolated to the enhancement layer.

## Color Pipeline

- Input probe extracts:
  - primaries
  - transfer
  - matrix
  - full/limited range
  - bit depth
  - pixel format
- SDR detection is explicit and based on transfer characteristics.
- HDR export forces a 10-bit path and stamps:
  - `bt2020` primaries
  - `smpte2084` transfer
  - `bt2020nc` matrix
- The pipeline rejects obviously inconsistent HDR requests instead of emitting a
  mislabeled file.

## Planned Performance Evolution

- Current:
  - FFmpeg raw frame pipe
  - host-memory bridge into RTX Video SDK
- Next:
  - FFmpeg `AVHWDeviceContext` integration
  - NVDEC/NVENC first-class path
  - reduced CPU-GPU round trips
- Later:
  - direct GPU frame interop for enhancement and encode

