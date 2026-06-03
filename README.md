# RTXHDR-RTXVSR

`RTXHDR-RTXVSR` 是一个面向 Windows 的本地桌面视频转换工具，目标是把 NVIDIA RTX Video VSR 超分辨率和 RTX Video HDR / TrueHDR 用到完整视频文件上，而不只是浏览器流媒体。

这个仓库当前主要提供源码、构建方式和打包流程，适合想自己编译、调试或继续开发的人使用。

## 这是什么

项目目前采用本地桌面栈：

- `C++20` 后端 sidecar，负责本地 HTTP API、媒体探测、能力检测、任务队列、取消控制，以及 `FFmpeg + D3D11 + NVENC + RTX Video SDK` 的实际处理流程。
- `React + Vite + TypeScript` 前端，负责输入文件选择、参数设置、状态提示、任务进度和错误展示。
- `Tauri` Windows 壳层，负责拉起本地后端 sidecar、调用原生文件/目录选择框，以及生成安装包和便携包。

后端只监听 `127.0.0.1`，不会暴露到局域网。

## 它现在能做什么

从当前源码实现来看，这个项目已经具备这些核心能力：

- 对本地视频文件做媒体探测，读取文件名、大小、分辨率、时长、编码等信息
- 检测当前机器是否具备 `D3D11`、`RTX Video SDK`、`NVENC H.264`、`NVENC HEVC Main10` 等处理能力
- 创建本地转码任务，并通过轮询方式展示进度、帧数、FPS、ETA
- 支持取消正在执行或排队中的任务
- 支持 `VSR`、`HDR`、`VSR + HDR` 三种处理模式
- 支持设置 VSR 放大倍率、VSR 质量、HDR 参数
- 支持保留或移除音频/字幕轨
- 支持 Windows 下的 `Tauri` 开发模式、安装包构建和便携版打包

## 适合谁

这个仓库更适合下面这几类人：

- 想做一个本地 RTX 视频增强桌面工具，并愿意自己编译源码
- 想基于 `FFmpeg + RTX Video SDK + Tauri` 这条技术路线继续开发
- 想研究“浏览器界面 + 本地 sidecar 后端 + Windows 打包”这一套实现方式

## 你需要先知道的事

- 这是一个 `Windows only` 项目
- 真正启用 RTX 硬件处理时，需要 `NVIDIA RTX GPU`、合适的驱动、支持 `D3D11VA` 和 `NVENC` 的 FFmpeg，以及 `NVIDIA RTX Video SDK 1.1.0`
- 仓库里不会提交构建产物、FFmpeg 二进制、SDK 运行库、示例视频或打好的安装包
- 如果你现在只是想验证源码结构、接口和前端页面，可以先用“不启用 FFmpeg / RTX SDK”的后端测试构建

## 环境要求

### 基础开发环境

- Windows 10 / 11
- CMake 3.25 或更高版本
- 支持 C++20 的编译器，通常是 Visual Studio 或 Visual Studio Build Tools
- Node.js 和 npm
- Rust 和 Cargo（用于 Tauri）

### 真正启用 RTX 视频处理时还需要

- 支持的 NVIDIA RTX 显卡
- 较新的 NVIDIA 驱动，并具备 RTX Video 支持
- Windows 版 FFmpeg 开发包，且包含头文件、导入库、`D3D11VA`、`NVENC`
- `NVIDIA RTX Video SDK 1.1.0`

默认情况下，后端会在下面这个目录查找 SDK：

```text
RTX_Video_SDK_v1.1.0/
```

如果你的 SDK 不在这里，也可以通过 `-DVSR_RTX_SDK_ROOT=...` 传入自定义路径。

## 快速开始

### 1. 只验证源码和界面

如果你暂时没有配好 FFmpeg、RTX SDK 或 RTX 显卡，可以先构建一个测试版后端：

```powershell
cmake -S backend -B build\backend -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
cmake --build build\backend --target vsr_backend_tests vsr_backend
ctest --test-dir build\backend --output-on-failure
```

这个模式下：

- 可以跑后端单元测试
- 可以联调前端和本地 API
- 不能执行真正的 RTX 硬件视频增强

### 2. 构建真正的硬件处理后端

先准备好 FFmpeg 开发包，然后设置 `FFMPEG_ROOT` 或直接通过 CMake 传入。

示例：

```powershell
cmake -S backend -B build\backend-hw -DVSR_ENABLE_TESTS=OFF -DVSR_ENABLE_FFMPEG=ON -DVSR_ENABLE_RTX_SDK=ON -DFFMPEG_ROOT="C:\ffmpeg"
cmake --build build\backend-hw --target vsr_backend --config Release
```

手动运行后端：

```powershell
.\build\backend-hw\Release\vsr_backend.exe --port 49321
```

## 前端开发

在前端目录下安装依赖并执行校验：

```powershell
cd frontend\rtx-video-converter
npm install
npm run lint
npm run test
npm run build
```

浏览器开发模式：

```powershell
npm run dev
```

注意：

- 浏览器开发模式下，请先手动启动后端
- 浏览器模式可以调用后端 API
- 浏览器模式不能使用 Tauri 的原生文件/目录选择框

## Tauri 桌面开发

如果你想跑真正的桌面应用壳层，需要先准备好后端 sidecar。

### 使用测试版后端 sidecar

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend\Debug\vsr_backend.exe
npm run tauri:dev
```

### 使用硬件版后端 sidecar

```powershell
cd frontend\rtx-video-converter
npm run sidecar:prepare -- -BackendExe ..\..\build\backend-hw\Release\vsr_backend.exe
npm run tauri:dev
```

`sidecar:prepare` 会把 `vsr_backend.exe` 和它旁边需要的运行时 DLL 一起复制到 Tauri 的打包布局里。

## Windows 打包

构建安装包：

```powershell
cd frontend\rtx-video-converter
npm run tauri:build
```

同时构建安装包和便携版：

```powershell
cd frontend\rtx-video-converter
npm run tauri:build:portable
```

构建完成后，产物通常会出现在：

```text
frontend/rtx-video-converter/src-tauri/target/release/bundle/nsis/
frontend/rtx-video-converter/src-tauri/target/release/bundle/portable/
```

这些产物默认不会提交到 git。

## 前端会调用哪些本地接口

前端当前通过本地后端调用这些接口：

- `GET /api/health`
- `GET /api/capabilities`
- `POST /api/media/probe`
- `POST /api/jobs`
- `GET /api/jobs/{id}`
- `POST /api/jobs/{id}/cancel`

更完整的请求/响应示例请看 [FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)。

## 仓库结构

```text
.
|-- backend/                         C++ 本地 sidecar 后端
|   |-- src/                         后端源码
|   |-- tests/                       单元测试与硬件 smoke 脚本
|   `-- cmake/                       CMake 辅助模块
|-- frontend/
|   `-- rtx-video-converter/         React + Vite + Tauri 前端
|-- docs/                            过程文档与实现记录
|-- FRONTEND_INTEGRATION.md          前后端接口约定
|-- PROJECT_CONTEXT.md               当前项目状态说明
`-- CMakeLists.txt                   工作区入口
```

## 对贡献者的说明

- 不要把构建产物提交进仓库
- 不要把大型 SDK、FFmpeg 二进制、视频样本和打包结果提交进仓库
- 没有硬件依赖时，优先用测试版后端验证源码和接口
- 需要输出真正可用的安装包或便携版时，再切换到硬件版后端构建
