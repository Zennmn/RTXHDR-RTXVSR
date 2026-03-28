[CmdletBinding()]
param(
  [switch]$Build,
  [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Resolve-FirstExistingPath {
  param([string[]]$Candidates)

  foreach ($candidate in $Candidates) {
    if ([string]::IsNullOrWhiteSpace($candidate)) {
      continue
    }
    if (Test-Path $candidate) {
      return (Resolve-Path $candidate).Path
    }
  }
  return $null
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$nativeRoot = Join-Path $repoRoot 'app\native'
$buildRoot = Join-Path $nativeRoot 'out\build'
$installRoot = Join-Path $nativeRoot 'out\install'

$cmakeExe = Resolve-FirstExistingPath @(
  $env:CMAKE_EXE,
  'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
)
if (-not $cmakeExe) {
  $cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue)?.Source
}
if (-not $cmakeExe) {
  throw 'Unable to locate cmake.exe.'
}

$ffmpegRoot = Resolve-FirstExistingPath @(
  $env:FFMPEG_ROOT,
  'C:\Program Files (x86)\ffmpeg-N-122395-g48c9c38684-win64-gpl-shared'
)
if (-not $ffmpegRoot) {
  throw 'Unable to locate FFmpeg root. Set FFMPEG_ROOT.'
}

$sdkRoot = Resolve-FirstExistingPath @(
  $env:NV_RTX_VIDEO_SDK,
  (Join-Path $repoRoot 'RTX_Video_SDK_v1.1.0')
)
if (-not $sdkRoot) {
  throw 'Unable to locate NVIDIA RTX Video SDK root.'
}

$codecRoot = Resolve-FirstExistingPath @(
  $env:NV_VIDEO_CODEC_INTERFACE,
  (Join-Path $repoRoot 'Video_Codec_Interface_13.0.37')
)
if (-not $codecRoot) {
  throw 'Unable to locate NVIDIA Video Codec Interface root.'
}

& $cmakeExe `
  -S $nativeRoot `
  -B $buildRoot `
  -G 'Visual Studio 18 2026' `
  -A x64 `
  "-DCMAKE_INSTALL_PREFIX=$installRoot" `
  "-DFFMPEG_ROOT=$ffmpegRoot" `
  "-DNV_RTX_VIDEO_SDK=$sdkRoot" `
  "-DNV_VIDEO_CODEC_INTERFACE=$codecRoot"

if ($Build) {
  & $cmakeExe --build $buildRoot --config $Configuration --target install
}

Write-Host "Native build tree: $buildRoot"
Write-Host "Native install tree: $installRoot"
