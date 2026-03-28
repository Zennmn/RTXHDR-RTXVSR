# Third Party Layout

This repository keeps dependency discovery logic and documentation in-tree, but
expects heavyweight third-party SDKs to live outside versioned source files.

- FFmpeg is discovered from `FFMPEG_ROOT` or the known local install path.
- NVIDIA RTX Video SDK is discovered from `NV_RTX_VIDEO_SDK` or the workspace
  copy.
- NVIDIA Video Codec Interface is discovered from `NV_VIDEO_CODEC_INTERFACE` or
  the workspace copy.

