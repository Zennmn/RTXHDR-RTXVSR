# Third-Party Components and Distribution Notes

This repository's original source code is licensed under the MIT License in [LICENSE](./LICENSE), unless a file states otherwise.

Third-party software, SDKs, libraries, binaries, drivers, and assets are not relicensed under MIT. They remain under their own terms.

## NVIDIA RTX Video SDK

- The repository does not vendor the NVIDIA RTX Video SDK source tree, headers, libraries, or runtime DLLs into git.
- Building with real RTX VSR / HDR support depends on the NVIDIA RTX Video SDK and its separate license terms.
- NVIDIA SDK materials are proprietary. Do not assume they become MIT-licensed just because this repository integrates with them.
- If you modify or distribute software that includes NVIDIA SDK materials, sample-derived code, or NVIDIA runtime binaries, you must comply with the NVIDIA RTX SDK license that applies to those materials.
- In particular, do not redistribute the SDK as a standalone product, and keep any required NVIDIA notices intact.

Reference:
- Official NVIDIA page: <https://developer.nvidia.com/rtx-video-sdk>
- Official NVIDIA RTX SDK license PDF: <https://developer.nvidia.com/downloads/nvidia-rtx-sdks-license-23jan2023pdf>
- If you downloaded the SDK locally, its package also includes an NVIDIA license PDF.

## FFmpeg

- This repository does not vendor FFmpeg source code or binaries into git.
- Release builds may bundle FFmpeg DLLs beside the backend executable, depending on how the local FFmpeg build is prepared.
- FFmpeg is generally available under LGPL 2.1-or-later, but some optional components are GPL-only or otherwise more restrictive.
- If you distribute builds that include FFmpeg binaries, you are responsible for complying with the license of the exact FFmpeg build you ship.

If you want the surrounding application to stay outside GPL obligations, use an LGPL-compatible FFmpeg build and follow FFmpeg's distribution checklist, including:

- use dynamic linking
- provide the corresponding FFmpeg source code for the binaries you distribute
- provide the FFmpeg build/configuration details
- include the required FFmpeg license notice

Official FFmpeg legal page:
- <https://www.ffmpeg.org/legal.html>

## Other Dependencies

- Frontend npm packages, Rust crates, and other fetched build dependencies remain under their own licenses.
- Review `frontend/rtx-video-converter/package.json`, `frontend/rtx-video-converter/package-lock.json`, and `frontend/rtx-video-converter/src-tauri/Cargo.lock` when preparing third-party notices for a packaged release.

## Practical Summary

- The repository's own code: MIT
- NVIDIA RTX Video SDK: proprietary, separate license
- FFmpeg: separate license, often LGPL but depends on the exact build
- Distributed app packages: may contain components under multiple licenses at the same time
