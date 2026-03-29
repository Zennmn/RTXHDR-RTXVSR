[CmdletBinding()]
param(
  [switch]$SkipFFmpeg,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$depsRoot = Join-Path $repoRoot 'deps'

Write-Host ''
Write-Host '======================================================'
Write-Host '  RTXHDR+RTXVSR Windows 开发环境自动准备'
Write-Host '======================================================'
Write-Host ''

# ─────────────────────────────────────────────
# 1. FFmpeg (LGPL shared)
# ─────────────────────────────────────────────
Write-Host '── 步骤 1/5: FFmpeg LGPL shared ──'
$ffmpegRoot = Join-Path $depsRoot 'ffmpeg'

if ($SkipFFmpeg) {
  Write-Host '[FFmpeg] 已跳过 (-SkipFFmpeg)'
} else {
  pwsh "$PSScriptRoot\fetch_ffmpeg.ps1" -TargetDir $ffmpegRoot | Out-Host
}

if (Test-Path (Join-Path $ffmpegRoot 'bin\ffmpeg.exe')) {
  $env:FFMPEG_ROOT = $ffmpegRoot
  Write-Host "[FFmpeg] FFMPEG_ROOT = $ffmpegRoot"
} elseif ($env:FFMPEG_ROOT -and (Test-Path (Join-Path $env:FFMPEG_ROOT 'bin\ffmpeg.exe'))) {
  Write-Host "[FFmpeg] 使用环境变量 FFMPEG_ROOT = $env:FFMPEG_ROOT"
} else {
  Write-Host '[FFmpeg] 警告: 未找到 FFmpeg，请设置 FFMPEG_ROOT 环境变量或重新运行此脚本。' -ForegroundColor Yellow
}

# ─────────────────────────────────────────────
# 2. NVIDIA RTX Video SDK
# ─────────────────────────────────────────────
Write-Host ''
Write-Host '── 步骤 2/5: NVIDIA RTX Video SDK ──'
$sdkInRepo = Join-Path $repoRoot 'RTX_Video_SDK_v1.1.0'
$sdkInDeps = Join-Path $depsRoot 'RTX_Video_SDK_v1.1.0'

$sdkRoot = if ($env:NV_RTX_VIDEO_SDK -and (Test-Path $env:NV_RTX_VIDEO_SDK)) {
  $env:NV_RTX_VIDEO_SDK
} elseif (Test-Path $sdkInRepo) {
  $sdkInRepo
} elseif (Test-Path $sdkInDeps) {
  $sdkInDeps
} else {
  $null
}

if ($sdkRoot) {
  $env:NV_RTX_VIDEO_SDK = $sdkRoot
  Write-Host "[RTX Video SDK] 已找到: $sdkRoot"
} else {
  Write-Host '[RTX Video SDK] 未找到。' -ForegroundColor Yellow
  Write-Host ''
  Write-Host '  NVIDIA RTX Video SDK 需要手动下载（需要 NVIDIA 开发者账号）：'
  Write-Host '  下载地址: https://developer.nvidia.com/rtx-video-sdk'
  Write-Host ''
  Write-Host '  下载后请执行以下任一操作：'
  Write-Host "  (a) 解压到: $sdkInRepo"
  Write-Host "  (b) 解压到: $sdkInDeps"
  Write-Host '  (c) 设置环境变量: $env:NV_RTX_VIDEO_SDK = "你的SDK路径"'
  Write-Host ''
}

# ─────────────────────────────────────────────
# 3. NVIDIA Video Codec Interface
# ─────────────────────────────────────────────
Write-Host '── 步骤 3/5: NVIDIA Video Codec Interface ──'
$vciInRepo = Join-Path $repoRoot 'Video_Codec_Interface_13.0.37'
$vciInDeps = Join-Path $depsRoot 'Video_Codec_Interface_13.0.37'

$vciRoot = if ($env:NV_VIDEO_CODEC_INTERFACE -and (Test-Path $env:NV_VIDEO_CODEC_INTERFACE)) {
  $env:NV_VIDEO_CODEC_INTERFACE
} elseif (Test-Path $vciInRepo) {
  $vciInRepo
} elseif (Test-Path $vciInDeps) {
  $vciInDeps
} else {
  $null
}

if ($vciRoot) {
  $env:NV_VIDEO_CODEC_INTERFACE = $vciRoot
  Write-Host "[Video Codec Interface] 已找到: $vciRoot"
} else {
  Write-Host '[Video Codec Interface] 未找到。' -ForegroundColor Yellow
  Write-Host ''
  Write-Host '  可以从 NVIDIA Developer 下载 Video Codec SDK 并提取 Interface 头文件：'
  Write-Host '  下载地址: https://developer.nvidia.com/video-codec-sdk'
  Write-Host ''
  Write-Host "  解压后将 Interface 目录结构放到: $vciInRepo"
  Write-Host '  或设置环境变量: $env:NV_VIDEO_CODEC_INTERFACE = "你的路径"'
  Write-Host ''
}

# ─────────────────────────────────────────────
# 4. CUDA Toolkit
# ─────────────────────────────────────────────
Write-Host '── 步骤 4/5: CUDA Toolkit ──'
$cudaPath = $env:CUDA_PATH
if ($cudaPath -and (Test-Path $cudaPath)) {
  Write-Host "[CUDA] 已检测到 CUDA Toolkit: $cudaPath"
} else {
  $defaultCuda = 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA'
  if (Test-Path $defaultCuda) {
    $latestCuda = Get-ChildItem $defaultCuda -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($latestCuda) {
      Write-Host "[CUDA] 已在默认路径找到: $($latestCuda.FullName)"
    }
  } else {
    Write-Host '[CUDA] 未检测到 CUDA Toolkit。' -ForegroundColor Yellow
    Write-Host '  下载地址: https://developer.nvidia.com/cuda-toolkit'
    Write-Host '  安装 CUDA Toolkit 后，CUDA_PATH 环境变量会自动设置。'
    Write-Host ''
  }
}

# ─────────────────────────────────────────────
# 5. Flutter SDK
# ─────────────────────────────────────────────
Write-Host '── 步骤 5/5: Flutter SDK ──'
$flutterRoot = if ($env:FLUTTER_ROOT) { $env:FLUTTER_ROOT } else { 'C:\Users\21826\develop\flutter' }
$flutterExe = Join-Path $flutterRoot 'bin\flutter.bat'

if (Test-Path $flutterExe) {
  Write-Host "[Flutter] 已找到: $flutterRoot"
} else {
  $flutterInPath = Get-Command flutter -ErrorAction SilentlyContinue
  if ($flutterInPath) {
    Write-Host "[Flutter] 已在 PATH 中找到 Flutter"
  } else {
    Write-Host '[Flutter] 未检测到 Flutter SDK。' -ForegroundColor Yellow
    Write-Host '  下载地址: https://docs.flutter.dev/get-started/install/windows/desktop'
    Write-Host '  安装后设置: $env:FLUTTER_ROOT = "你的Flutter路径"'
    Write-Host ''
  }
}

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────
Write-Host ''
Write-Host '======================================================'
Write-Host '  环境检查结果'
Write-Host '======================================================'

$checks = @(
  @{ Name = 'FFmpeg';         Found = (Test-Path (Join-Path ($env:FFMPEG_ROOT ?? '') 'bin\ffmpeg.exe')) },
  @{ Name = 'RTX Video SDK';  Found = ($null -ne $sdkRoot) },
  @{ Name = 'Video Codec IF'; Found = ($null -ne $vciRoot) },
  @{ Name = 'CUDA Toolkit';   Found = ($null -ne $cudaPath -and (Test-Path ($cudaPath ?? ''))) },
  @{ Name = 'Flutter SDK';    Found = (Test-Path $flutterExe) }
)

$allReady = $true
foreach ($check in $checks) {
  $status = if ($check.Found) { '  ✅' } else { '  ❌'; $allReady = $false }
  Write-Host "$status $($check.Name)"
}

Write-Host ''

if ($allReady) {
  Write-Host '所有依赖已就绪！' -ForegroundColor Green
  if (-not $SkipBuild) {
    Write-Host ''
    Write-Host '正在执行原生构建 ...'
    pwsh "$PSScriptRoot\configure_native.ps1" -Build | Out-Host
    Write-Host ''
    Write-Host '环境准备完成。可以运行: pwsh ./app/scripts/run_flutter_windows.ps1' -ForegroundColor Green
  }
} else {
  Write-Host '部分依赖缺失，请根据上方提示完成安装后重新运行此脚本。' -ForegroundColor Yellow
}
