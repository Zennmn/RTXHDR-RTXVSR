# RTXHDR-RTXVSR

`RTXHDR-RTXVSR` 是一个面向 Windows 的本地桌面视频转换工具，用于把 `NVIDIA RTX Video VSR` 超分辨率和 `RTX Video HDR / TrueHDR` 应用到完整视频文件。

## 下载

- 安装版：[RTX.Video.Converter_1.0.1_x64-setup.exe](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v1.0.1/RTX.Video.Converter_1.0.1_x64-setup.exe)
- 便携版：[RTX.Video.Converter_1.0.1_x64-portable.zip](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v1.0.1/RTX.Video.Converter_1.0.1_x64-portable.zip)
- FFmpeg 完整对应源码与构建配置：[RTX.Video.Converter_1.0.1_FFmpeg-corresponding-source.zip](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v1.0.1/RTX.Video.Converter_1.0.1_FFmpeg-corresponding-source.zip)
- 校验文件：[SHA256SUMS.txt](https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v1.0.1/SHA256SUMS.txt)

安装版与便携版包含动态链接的 FFmpeg LGPL 共享库以及受 NVIDIA 独立条款约束的 RTX Video SDK 材料。安装、使用或再分发前请阅读包内 `DISTRIBUTION_TERMS.txt`；完整的 .NET、Windows App SDK、NuGet 与 C++ 依赖声明收录在 `THIRD_PARTY_NOTICES.txt` 和 `THIRD_PARTY_LICENSES/`，本项目 MIT 许可证不覆盖这些第三方组件。

## 功能

- RTX VSR 视频超分
- RTX HDR / TrueHDR 映射
- VSR + HDR 组合处理
- 本地视频文件探测与输出设置
- NVENC H.264、HEVC/H.265 与 AV1 输出（HDR 使用 HEVC Main10 或 AV1 10-bit）
- 转码进度、FPS、ETA 显示
- 任务取消

## 运行要求

- Windows 10 / 11
- NVIDIA RTX 显卡
- 较新的 NVIDIA 驱动，并具备 RTX Video 支持

## 项目说明

- 这是一个 `Windows only` 项目
- 处理目标是完整视频文件，不是浏览器在线视频流
- 应用采用 `C++ sidecar 后端 + WinUI 3 前端` 的本地桌面架构
- 后端仅监听 `127.0.0.1`

## 许可证

- 仓库自有代码采用 [MIT License](./LICENSE)
- `NVIDIA RTX Video SDK`、FFmpeg 及其他第三方组件仍然使用各自许可证
- 安装/分发条款见 [DISTRIBUTION_TERMS.txt](./DISTRIBUTION_TERMS.txt)，具体组件边界见 [THIRD_PARTY.md](./THIRD_PARTY.md)
- NVIDIA 外部通知、商标批准和编解码专利评估不能由代码自动取得，发布前按 [NVIDIA release checklist](./docs/NVIDIA_RELEASE_CHECKLIST.md) 留存证据

## 从源码构建

如果你想自己开发、调试或修改项目，可以从源码构建。

基础环境：

- CMake 3.25+
- Visual Studio 或 Visual Studio Build Tools
- .NET 8 SDK

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
.\build-winui.ps1

# 同时生成自包含 WinUI 发布目录，并装配指定硬件后端
.\build-winui.ps1 -Publish -BackendExe .\build\backend-hw\Release\vsr_backend.exe

# 从上述发布目录生成便携版与 Inno Setup 安装版
.\package-release.ps1 -Version 1.0.1
```


## 相关文档

- 接口说明：[FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)
- 项目上下文：[PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md)
