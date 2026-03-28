# Native API

The native DLL exports a stable C ABI for Flutter FFI.

## Lifecycle

- `rtx_init_library`
- `rtx_shutdown_library`

## System And Media Probe

- `rtx_probe_system`
- `rtx_list_supported_codecs`
- `rtx_analyze_input`

## Job Lifecycle

- `rtx_create_job`
- `rtx_start_job`
- `rtx_query_job_progress`
- `rtx_cancel_job`
- `rtx_destroy_job`
- `rtx_drain_job_events`

## Error Reporting

- APIs return `0` for success and non-zero for failure.
- `rtx_get_last_error` returns the latest library-level message for the calling
  flow.
- Job failures are also reflected in:
  - `RtxJobProgressV1.status_message`
  - drained log events

## ABI Design Notes

- No C++ class types cross the boundary.
- Strings in returned structs use fixed-size UTF-8 buffers.
- Job handles are opaque `uint64_t` ids owned by the native library.
- Polling is used instead of callbacks so Flutter remains simple and safe.

