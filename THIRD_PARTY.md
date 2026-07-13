# Third-Party Components and Distribution Notes

This repository's original source code is licensed under the MIT License in [LICENSE](./LICENSE), unless a file states otherwise.

Third-party software, SDKs, libraries, binaries, drivers, and assets are not relicensed under MIT. They remain under their own terms.

## NVIDIA RTX Video SDK

- The repository does not vendor the NVIDIA RTX Video SDK source tree, headers, libraries, or runtime DLLs into git.
- Release packages incorporate NVIDIA RTX Video SDK object code and the `nvngx_vsr.dll` / `nvngx_truehdr.dll` runtime materials under NVIDIA's separate license terms.
- NVIDIA SDK materials are proprietary. Do not assume they become MIT-licensed just because this repository integrates with them.
- If you modify or distribute software that includes NVIDIA SDK materials, sample-derived code, or NVIDIA runtime binaries, you must comply with the NVIDIA RTX SDK license that applies to those materials.
- In particular, do not redistribute the SDK as a standalone product, and keep any required NVIDIA notices intact.
- The SDK license does not grant audio/video codec patent rights. Commercial distributors must evaluate the patent licensing requirements for their codecs, territories, and use cases.
- NVIDIA mark attribution, release notification, and any required written trademark approval are external release prerequisites; see `docs/NVIDIA_RELEASE_CHECKLIST.md`.

Reference:
- Official NVIDIA page: <https://developer.nvidia.com/rtx-video-sdk>
- Official NVIDIA RTX SDK license PDF: <https://developer.nvidia.com/downloads/nvidia-rtx-sdks-license-23jan2023pdf>
- If you downloaded the SDK locally, its package also includes an NVIDIA license PDF.

## FFmpeg

- This repository does not vendor FFmpeg source code or binaries into git.
- Release 1.0.0 bundles unmodified FFmpeg shared libraries built from commit `a09be9b91e8e1219f297586873b0d7322b47df96`.
- The build uses only FFmpeg's built-in LGPL code plus MIT-licensed `nv-codec-headers` commit `15ee32753c92faddbabbff11676779618fc6db7e`.
- GPL, nonfree, libx264, libx265, and other optional third-party codec libraries are not enabled.
- The reproducible recipe is `third_party/ffmpeg/Dockerfile`; `build-minimal-ffmpeg.ps1` produces the exact source archives and complete configure log.
- The corresponding-source archive is published beside each binary release. For v1.0.0 it is `RTX.Video.Converter_1.0.0_FFmpeg-corresponding-source.zip`.
- The application dynamically links to these DLLs. Users may replace them with ABI-compatible modified FFmpeg builds.

Release builders must keep the pinned LGPL-compatible recipe and follow FFmpeg's distribution checklist, including:

- use dynamic linking
- provide the corresponding FFmpeg source code for the binaries you distribute
- provide the FFmpeg build/configuration details
- include the required FFmpeg license notice

Official FFmpeg legal page:
- <https://www.ffmpeg.org/legal.html>

## Other Dependencies

- WinUI NuGet packages, including Microsoft Windows App SDK, CommunityToolkit.Mvvm, and Microsoft.Extensions.DependencyInjection, remain under their own licenses.
- CMake-fetched dependencies, including cpp-httplib, nlohmann/json, and GoogleTest, remain under their own licenses.
- Release packages include `THIRD_PARTY_NOTICES.txt` plus the original,
  versioned license and notice files in `THIRD_PARTY_LICENSES/` for the .NET
  Runtime, Windows SDK/App SDK, NuGet runtime graph, cpp-httplib, and
  nlohmann/json.
- `generate-third-party-notices.ps1` regenerates the inventory from the NuGet
  lock file, restored packages, configured CMake dependencies, published .NET
  runtime, and installed Windows SDK. The release compliance check verifies
  every listed file against its SHA-256 value.
- GoogleTest is a test-only dependency and is not distributed in the release.

## Practical Summary

- The repository's own code: MIT
- NVIDIA RTX Video SDK: proprietary, separate license
- FFmpeg: separate license, often LGPL but depends on the exact build
- Distributed app packages: may contain components under multiple licenses at the same time
- The package-level terms shown by the installer are in `DISTRIBUTION_TERMS.txt`
