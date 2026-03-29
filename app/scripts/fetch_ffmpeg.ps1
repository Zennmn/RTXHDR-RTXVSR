[CmdletBinding()]
param(
  [string]$TargetDir = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $TargetDir) {
  $TargetDir = Join-Path $repoRoot 'deps\ffmpeg'
}

$zipName = 'ffmpeg-n7.1-latest-win64-lgpl-shared-7.1.zip'
$downloadUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/$zipName"

if (Test-Path (Join-Path $TargetDir 'bin\ffmpeg.exe')) {
  Write-Host "[FFmpeg] 已存在于 $TargetDir，跳过下载。"
  Write-Host "[FFmpeg] 如需强制重新下载，请删除该目录后重试。"
  return
}

Write-Host "[FFmpeg] 正在从 BtbN GitHub Releases 下载 FFmpeg LGPL shared ..."
Write-Host "[FFmpeg] URL: $downloadUrl"
Write-Host "[FFmpeg] 注意: 这是 LGPL 2.1+ 许可的 FFmpeg 构建（非 GPL），不会污染 MIT 许可的项目代码。"

$tempDir = Join-Path $repoRoot 'deps\.tmp'
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
$zipPath = Join-Path $tempDir $zipName

try {
  $ProgressPreference = 'SilentlyContinue'
  Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -UseBasicParsing
  $ProgressPreference = 'Continue'
  Write-Host "[FFmpeg] 下载完成 ($([math]::Round((Get-Item $zipPath).Length / 1MB, 1)) MB)"
} catch {
  throw "[FFmpeg] 下载失败: $_`n请检查网络连接或手动下载: $downloadUrl"
}

Write-Host "[FFmpeg] 正在解压 ..."
$extractDir = Join-Path $tempDir 'ffmpeg_extract'
if (Test-Path $extractDir) { Remove-Item -Recurse -Force $extractDir }
Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force

$innerDir = Get-ChildItem -Path $extractDir -Directory | Select-Object -First 1
if (-not $innerDir) {
  throw "[FFmpeg] 解压后未找到有效目录。"
}

New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
Copy-Item -Path (Join-Path $innerDir.FullName '*') -Destination $TargetDir -Recurse -Force

Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue

Write-Host "[FFmpeg] 安装完成: $TargetDir"
Write-Host "[FFmpeg] 许可: LGPL 2.1+ (shared build from BtbN/FFmpeg-Builds)"
Write-Host "[FFmpeg] 源码: https://github.com/FFmpeg/FFmpeg"
