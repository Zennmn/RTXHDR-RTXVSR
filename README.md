# RTXHDR+RTXVSR

`RTXHDR+RTXVSR` 是一个面向 Windows 与 NVIDIA RTX 平台的离线视频增强工具。
桌面界面使用 Flutter 开发，原生处理核心使用 C++、CMake、FFmpeg 与 NVIDIA RTX Video SDK。

本项目的目标是把本地视频文件导入后，离线执行以下增强能力，并导出为全新文件：

- RTX Video Super Resolution 视频超分
- NVIDIA TrueHDR SDR 转 HDR
- HEVC / AV1 硬件编码导出
- 任务进度、日志、ETA 与错误信息可视化

## 项目特点

- Windows 桌面 GUI，适合本地离线批处理
- Flutter 通过 `dart:ffi` 调用原生 DLL
- 使用 FFmpeg 探测媒体信息、解码、封装与编码
- NVIDIA 增强逻辑封装在独立后端层，便于后续替换或扩展
- 已支持基础的导出参数记忆功能，会自动保存上次使用的选项

## 当前能力范围

- 支持导入 `mp4`、`mkv`、`mov`
- 探测分辨率、帧率、编码格式、像素格式、色彩信息、位深与音频流
- 可选择保持分辨率、2 倍超分或自定义输出分辨率
- 可配置音频保留策略、码率控制方式、HDR 参数
- 支持任务创建、轮询进度、取消任务、查看详细日志

## 技术架构

### Flutter 层

路径：`app/flutter_app`

- Windows 桌面 UI
- 任务参数编辑面板
- 系统能力探测面板
- 导出任务进度和日志面板
- FFI 调用原生 `rtx_native.dll`

### Native 层

路径：`app/native`

- `core`
  - 状态码、日志、公共类型、配置校验
- `video_io`
  - FFmpeg 运行时发现、媒体探测、硬件编解码能力探测
- `enhance`
  - 增强后端抽象、空实现后端、NVIDIA CUDA/NGX 后端
- `pipeline`
  - 作业管理、离线处理管线、进度回报、取消控制
- `platform`
  - Windows 能力探测、稳定 C ABI、Flutter FFI 入口

## 目录结构

```text
.
├─ app/
│  ├─ docs/                 文档
│  ├─ flutter_app/          Flutter Windows 桌面应用
│  ├─ native/               C++ 原生处理核心
│  └─ scripts/              构建、运行、打包脚本
├─ .clang-format
├─ .editorconfig
└─ README.md
```

## 运行环境

- Windows 10 / 11 x64
- NVIDIA RTX 显卡
- 建议使用较新的 NVIDIA 驱动
- Flutter SDK
- Visual Studio Build Tools with CMake
- FFmpeg 共享库版本
- CUDA Toolkit
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface

## 依赖说明

为了避免将体积较大、许可独立或不适合直接再分发的文件提交到源码仓库，本仓库默认不包含以下依赖内容：

- FFmpeg 安装目录
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface
- 本地构建产物与发布包

你需要在本机准备这些依赖，并通过环境变量或默认路径让项目发现它们：

- `FFMPEG_ROOT`
- `NV_RTX_VIDEO_SDK`
- `NV_VIDEO_CODEC_INTERFACE`
- `FLUTTER_ROOT`
- `CMAKE_EXE`
- `RTXHDR_NATIVE_DLL`

## 本地构建

### 1. 配置并构建原生 DLL

```powershell
pwsh ./app/scripts/configure_native.ps1 -Build
```

### 2. 运行 Flutter Windows 应用

```powershell
pwsh ./app/scripts/run_flutter_windows.ps1
```

### 3. 构建便携版发布目录

```powershell
pwsh ./app/scripts/package_runtime.ps1
```

发布产物默认输出到：

- `dist/RTXHDR_RTXVSR_Windows_Portable/`

## 文档

- `app/docs/architecture.md`
- `app/docs/build_windows.md`
- `app/docs/native_api.md`
- `app/docs/troubleshooting.md`

## 当前限制

- 当前版本重点是单任务离线处理流程
- 字幕、章节等高级封装信息尚未完整拷贝
- 目前仍是 FFmpeg raw-frame + 主机内存中转方案，不是最终零拷贝 GPU 管线
- MP4 音频直通策略较保守，必要时会回退为 AAC
- 项目目前主要面向 Windows + NVIDIA RTX 环境验证

## 开源许可

本仓库源码采用 MIT License。

请注意：

- 本仓库源码许可不自动覆盖第三方依赖的许可
- 使用 FFmpeg、CUDA、NVIDIA RTX Video SDK、Video Codec Interface 时，需要你自行遵守对应上游许可与再分发条款

## 致谢

- Flutter
- FFmpeg
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface
