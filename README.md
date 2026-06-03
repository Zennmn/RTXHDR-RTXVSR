# RTXHDR-RTXVSR

`RTXHDR-RTXVSR` 是一个面向 Windows 的本地桌面视频转换工具，用于把 `NVIDIA RTX Video VSR` 超分辨率和 `RTX Video HDR / TrueHDR` 应用到完整视频文件。

## 下载

当前已提供现成发布包，直接下载即可：

- 安装版：[RTX.Video.Converter_0.1.0_x64-setup.exe](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v0.1.0/RTX.Video.Converter_0.1.0_x64-setup.exe)
- 便携版：[RTX.Video.Converter_0.1.0_x64-portable.zip](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v0.1.0/RTX.Video.Converter_0.1.0_x64-portable.zip)
- 所有版本：[Releases](https://github.com/Zennmn/RTXHDR-RTXVSR/releases)

## 功能

- RTX VSR 视频超分
- RTX HDR / TrueHDR 映射
- VSR + HDR 组合处理
- 本地视频文件探测与输出设置
- 转码进度、FPS、ETA 显示
- 任务取消

## 运行要求

- Windows 10 / 11
- NVIDIA RTX 显卡
- 较新的 NVIDIA 驱动，并具备 RTX Video 支持

## 项目说明

- 这是一个 `Windows only` 项目
- 处理目标是完整视频文件，不是浏览器在线视频流
- 应用采用 `C++ 后端 + React/Tauri 前端` 的本地桌面架构
- 后端仅监听 `127.0.0.1`

## 从源码构建

如果你想自己开发、调试或修改项目，可以从源码构建。

基础环境：

- CMake 3.25+
- Visual Studio 或 Visual Studio Build Tools
- Node.js / npm
- Rust / Cargo

如果要启用真正的 RTX 硬件处理，还需要：

- 支持 `D3D11VA` 和 `NVENC` 的 FFmpeg 开发包
- `NVIDIA RTX Video SDK 1.1.0`

后端默认会从下面这个目录查找 SDK：

```text
RTX_Video_SDK_v1.1.0/
```

简要构建流程：

```powershell
cmake -S backend -B build\backend-hw -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT="C:\ffmpeg"
cmake --build build\backend-hw --target vsr_backend --config Release
cd frontend\rtx-video-converter
npm install
npm run tauri:build:portable
```

## 相关文档

- 接口说明：[FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)
- 项目上下文：[PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md)
