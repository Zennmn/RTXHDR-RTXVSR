# RTXHDR+RTXVSR

`RTXHDR+RTXVSR` 是一个面向 Windows 与 NVIDIA RTX 平台的离线视频增强工具。  
桌面界面使用 Flutter 开发，原生处理核心使用 C++、CMake、FFmpeg 与 NVIDIA RTX Video SDK。

项目目标是让用户导入本地视频后，离线执行以下增强能力，并导出为全新文件：

- RTX Video Super Resolution 视频超分
- NVIDIA TrueHDR SDR 转 HDR
- HEVC / AV1 硬件编码导出
- 任务进度、日志、ETA 与错误信息可视化

## 下载

前往 [Releases](https://github.com/Zennmn/RTXHDR-RTXVSR/releases) 页面下载最新的 Windows 便携版。

**系统要求：**

- Windows 10 / 11 x64
- NVIDIA RTX 显卡
- 较新版本的 NVIDIA 显卡驱动

下载后解压即可运行，无需安装。
## 项目状态

这是一个偏工程原型阶段的 Windows 桌面项目，已经具备完整的基本链路：

- Flutter Windows 图形界面
- FFI 调用原生 DLL
- FFmpeg 媒体探测、解码、编码、封装
- NVIDIA CUDA / NGX 增强后端
- 导出参数自动记忆
- 基础测试与打包脚本

当前仓库适合：

- 想自己搭建环境并从源码运行的开发者
- 想研究 Flutter + FFI + C++ 视频处理架构的开发者
- 想在本地继续扩展 RTX 视频增强工作流的开发者

## 当前开源发布方式

本仓库当前采用“方案 A：仅发布源码”的方式开源。

也就是说：

- GitHub 仓库和 Release 默认只提供源码
- 不默认提供可直接运行的公开二进制包
- 使用者需要自行准备 FFmpeg、CUDA、NVIDIA RTX Video SDK、Video Codec Interface 等依赖

这样做的主要原因是第三方运行时和 SDK 的许可、再分发条款需要分别遵守。  
源码仓库使用 MIT License，但这并不自动覆盖第三方组件。

## 功能概览

- 支持导入 `mp4`、`mkv`、`mov`
- 探测分辨率、帧率、编码格式、像素格式、色彩信息、位深与音频流
- 支持保持原分辨率、2 倍超分或自定义输出分辨率
- 支持音频保留策略、码率控制方式、HDR 参数设置
- 支持系统能力探测、任务进度轮询、日志查看与取消任务

## 技术架构

### Flutter 层

路径：`app/flutter_app`

- Windows 桌面 UI
- 任务参数编辑面板
- 系统能力探测面板
- 导出任务进度与日志面板
- 通过 `dart:ffi` 调用 `rtx_native.dll`

### Native 层

路径：`app/native`

- `core`
  - 状态码、日志、公共类型、配置校验
- `video_io`
  - FFmpeg 运行时发现、媒体探测、硬件编解码能力探测
- `enhance`
  - 增强后端抽象、空实现后端、NVIDIA CUDA / NGX 后端
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
├─ .gitignore
├─ LICENSE
└─ README.md
```

## 运行环境要求

### 操作系统与硬件

- Windows 10 / 11 x64
- NVIDIA RTX 显卡
- 建议使用较新的 NVIDIA 驱动

### 开发工具

- Flutter SDK
- Visual Studio Build Tools with CMake
- PowerShell 7 或兼容 PowerShell 环境

### 第三方依赖

- FFmpeg 共享库版本
- CUDA Toolkit
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface

## 仓库为什么不包含第三方依赖

为了避免将体积较大、许可独立或不适合直接再分发的文件提交到源码仓库，本仓库默认不包含以下内容：

- FFmpeg 安装目录
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface
- 本地构建产物
- 便携版发布目录

也就是说，克隆本仓库后，你还需要自行准备这些依赖。

## 第一次使用前需要准备什么

建议先准备下面四类东西：

1. Flutter SDK
2. Visual Studio Build Tools 和 CMake
3. FFmpeg 共享库构建
4. NVIDIA 相关 SDK 与工具链

如果你已经具备这些依赖，最省心的方式是通过环境变量告诉项目这些工具在哪里。

## 推荐环境变量

本项目支持以下环境变量：

- `FLUTTER_ROOT`
- `CMAKE_EXE`
- `FFMPEG_ROOT`
- `NV_RTX_VIDEO_SDK`
- `NV_VIDEO_CODEC_INTERFACE`
- `RTXHDR_NATIVE_DLL`

其中最重要的通常是这几个：

- `FFMPEG_ROOT`
- `NV_RTX_VIDEO_SDK`
- `NV_VIDEO_CODEC_INTERFACE`
- `FLUTTER_ROOT`

## PowerShell 环境变量示例

你可以在当前 PowerShell 会话里先这样设置：

```powershell
$env:FLUTTER_ROOT = 'C:\dev\flutter'
$env:FFMPEG_ROOT = 'D:\tools\ffmpeg-shared'
$env:NV_RTX_VIDEO_SDK = 'D:\sdk\RTX_Video_SDK_v1.1.0'
$env:NV_VIDEO_CODEC_INTERFACE = 'D:\sdk\Video_Codec_Interface_13.0.37'
$env:CMAKE_EXE = 'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
```

如果你希望长期生效，可以把这些环境变量写入系统环境变量，或者根据你自己的开发环境封装一个本地启动脚本。

## 快速开始（推荐）

```powershell
git clone https://github.com/Zennmn/RTXHDR-RTXVSR.git
cd RTXHDR-RTXVSR
pwsh ./app/scripts/bootstrap_windows.ps1
```

`bootstrap_windows.ps1` 会自动完成：

- ✅ 下载 FFmpeg LGPL shared 构建（从 BtbN GitHub Releases）
- ⚠️ 检测 NVIDIA RTX Video SDK（需手动下载，脚本会给出指引）
- ⚠️ 检测 NVIDIA Video Codec Interface（需手动下载，脚本会给出指引）
- ✅ 检测 CUDA Toolkit
- ✅ 检测 Flutter SDK
- ✅ 自动执行原生构建

唯一需要手动操作的是从 NVIDIA 开发者网站下载 RTX Video SDK（需登录账号）。

## 从源码运行教程（手动方式）

如果你更喜欢手动配置，以下是完整流程。

### 1. 克隆仓库

```powershell
git clone https://github.com/Zennmn/RTXHDR-RTXVSR.git
cd RTXHDR-RTXVSR
```

### 2. 准备第三方依赖

你需要确保以下目录在本机真实存在：

- Flutter SDK 根目录
- FFmpeg 根目录（**建议使用 LGPL shared 构建**）
  - 应包含 `include`、`lib`、`bin`
- NVIDIA RTX Video SDK 根目录
- NVIDIA Video Codec Interface 根目录

依赖可以放在项目根目录或 `deps/` 目录下，脚本会自动搜索。
也可以通过环境变量显式指定路径。

### 3. 构建原生 DLL

```powershell
pwsh ./app/scripts/configure_native.ps1 -Build
```

正常情况下，构建完成后会得到：

- `app/native/out/install/bin/rtx_native.dll`

以及所需的 NGX 运行时 DLL 会被安装到同一目录中。

### 4. 运行 Flutter Windows 客户端

```powershell
pwsh ./app/scripts/run_flutter_windows.ps1
```

这个脚本会自动做几件事：

- 如果需要，先构建原生 DLL
- 设置 `RTXHDR_NATIVE_DLL`
- 把 FFmpeg 和 NVIDIA 运行时目录加入当前进程的 `PATH`
- 启动 Flutter Windows 应用

### 5. 首次启动后建议检查的内容

进入应用后，建议先看“系统能力”面板，确认这些内容是正常的：

- 显卡型号已识别
- 驱动版本已识别
- CUDA 版本已识别
- `NGX VSR` 可用
- `TrueHDR` 可用
- 至少一种硬件编码能力可用

如果这些信息异常，通常不是 Flutter 层问题，而是依赖路径或运行时环境没有准备完整。

## 一个更稳妥的使用顺序

如果你是第一次搭这个项目，建议按下面顺序排查：

1. 先执行原生构建
2. 确认 `app/native/out/install/bin/rtx_native.dll` 存在
3. 再运行 Flutter 客户端
4. 在 UI 里先测试“系统能力探测”
5. 再导入一个短视频做导出测试

这样比一上来直接跑完整导出更容易定位问题。

## 常用命令速查

### 原生配置

```powershell
pwsh ./app/scripts/configure_native.ps1
```

### 原生配置并构建

```powershell
pwsh ./app/scripts/configure_native.ps1 -Build
```

### 运行 Flutter Windows 应用

```powershell
pwsh ./app/scripts/run_flutter_windows.ps1
```

### Flutter 静态检查

```powershell
cd app/flutter_app
flutter analyze
```

### Flutter 测试

```powershell
cd app/flutter_app
flutter test
```

### 原生测试

```powershell
cd app/native/out/build
ctest -C Release --output-on-failure
```

## 如果你只想看源码，不想立刻搭环境

建议先从下面这些文件开始读：

- `app/docs/architecture.md`
- `app/docs/native_api.md`
- `app/native/platform/src/native_api.cpp`
- `app/native/pipeline/src/processing_pipeline.cpp`
- `app/flutter_app/lib/core/ffi/native_library.dart`
- `app/flutter_app/lib/features/job/application/export_job_controller.dart`

## 如果你想自己做本地便携包

仓库里保留了本地打包脚本，方便开发者在自己机器上生成便携版目录：

```powershell
pwsh ./app/scripts/package_runtime.ps1
```

默认输出目录：

- `dist/RTXHDR_RTXVSR_Windows_Portable/`

但是请注意：

- 这更适合你在本机自测或私下分发
- 是否适合公开上传二进制，请先确认第三方依赖的再分发许可

## 常见问题

### 1. 找不到 Flutter 或 CMake

优先检查：

- `FLUTTER_ROOT`
- `CMAKE_EXE`

如果脚本默认路径和你的机器不一致，请显式设置环境变量。

### 2. NGX 不可用

通常检查：

- `NV_RTX_VIDEO_SDK` 是否指向正确的 SDK 根目录
- SDK 的 `bin\Windows\x64\rel` 是否存在
- 当前进程的 `PATH` 是否包含该目录

### 3. FFmpeg 探测输入失败

通常检查：

- 输入文件路径是否真实存在
- `FFMPEG_ROOT` 是否指向共享库版本的 FFmpeg
- `bin` 目录下的 FFmpeg 运行时 DLL 是否可访问

### 4. 任务取消后残留临时文件

当前导出管线会先写入临时输出文件。  
如果任务失败或中断后有残留文件，一般可以手动删除。

### 5. HDR 输出结果不对

建议检查：

- 输出编码是否为 `HEVC Main10` 或 `AV1 10-bit`
- 是否启用了 HDR 导出
- 播放器是否真的识别 HDR 元数据

## 文档索引

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
- 完整的第三方许可声明请参见 [THIRD_PARTY_LICENSES.md](./THIRD_PARTY_LICENSES.md)

## 第三方组件

| 组件 | 许可证 | 说明 |
|------|--------|------|
| [FFmpeg](https://ffmpeg.org/) | LGPL 2.1+ | 动态链接，源码见 [GitHub](https://github.com/FFmpeg/FFmpeg) |
| [NVIDIA CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) | NVIDIA EULA | GPU 计算 |
| [NVIDIA RTX Video SDK](https://developer.nvidia.com/rtx-video-sdk) | NVIDIA SDK License | VSR + TrueHDR |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) | MIT | 硬件编解码接口头文件 |
| [Flutter](https://flutter.dev/) | BSD 3-Clause | 桌面 UI 框架 |

本项目并非由 NVIDIA Corporation 赞助或认可。

## 致谢

- Flutter
- FFmpeg
- NVIDIA RTX Video SDK
- NVIDIA Video Codec Interface
