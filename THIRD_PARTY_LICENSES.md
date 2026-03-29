# Third-Party Licenses

本项目源码采用 [MIT License](./LICENSE)。

以下第三方组件各自遵守其原始许可，本项目的 MIT 许可不覆盖这些组件。

---

## FFmpeg

- **项目地址**: https://ffmpeg.org/
- **许可证**: LGPL 2.1+（本项目使用动态链接的共享库构建）
- **源码地址**: https://github.com/FFmpeg/FFmpeg
- **构建来源**: https://github.com/BtbN/FFmpeg-Builds (LGPL shared build)

本项目通过子进程方式调用 FFmpeg，并使用其共享库进行链接编译。
根据 LGPL 2.1+ 要求，FFmpeg 源码可在上述地址获取。

---

## NVIDIA CUDA Toolkit

- **项目地址**: https://developer.nvidia.com/cuda-toolkit
- **许可证**: NVIDIA CUDA Toolkit EULA
- **完整许可**: https://docs.nvidia.com/cuda/eula/index.html

---

## NVIDIA RTX Video SDK

- **项目地址**: https://developer.nvidia.com/rtx-video-sdk
- **许可证**: NVIDIA Software License Agreement
- **完整许可**: 包含在 SDK 下载包中的 `NVIDIA_RTX_Video_SDK_License.pdf`

本项目使用 RTX Video SDK 提供的 NGX API 实现视频超分辨率（VSR）和
TrueHDR 功能。SDK 运行时 DLL（`nvngx_vsr.dll`、`nvngx_truehdr.dll`）
仅以嵌入方式随应用分发。

> 注意: 此 SDK 需要从 NVIDIA 开发者网站手动下载，不包含在本仓库中。

---

## NVIDIA Video Codec Interface (nv-codec-headers)

- **项目地址**: https://github.com/FFmpeg/nv-codec-headers
- **许可证**: MIT License

本项目使用 Video Codec Interface 头文件（`nvEncodeAPI.h`、`cuviddec.h`、
`nvcuvid.h`）与 NVIDIA 硬件编解码器交互。

---

## Flutter

- **项目地址**: https://flutter.dev/
- **许可证**: BSD 3-Clause License
- **源码地址**: https://github.com/flutter/flutter

---

## 声明

- 本项目（RTXHDR+RTXVSR）并非由 NVIDIA Corporation 赞助或认可。
- 使用者有责任自行遵守上述各第三方组件的许可条款。
- 如有任何许可疑问，请参阅各组件的官方许可文件。
