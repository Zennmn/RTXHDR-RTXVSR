[CmdletBinding()]
param(
  [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$flutterRoot = if ($env:FLUTTER_ROOT) { $env:FLUTTER_ROOT } else { 'C:\Users\21826\develop\flutter' }
$flutterExe = Join-Path $flutterRoot 'bin\flutter.bat'

if (-not (Test-Path $flutterExe)) {
  throw "Unable to locate flutter.bat at $flutterExe"
}

pwsh "$PSScriptRoot\configure_native.ps1" -Build -Configuration $Configuration | Out-Host

$nativeDll = Join-Path $repoRoot 'app\native\out\install\bin\rtx_native.dll'
$ffmpegCandidates = @(
  $env:FFMPEG_ROOT,
  (Join-Path $repoRoot 'deps\ffmpeg'),
  'C:\Program Files (x86)\ffmpeg-N-122395-g48c9c38684-win64-gpl-shared'
)
$ffmpegResolved = $ffmpegCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$ffmpegBin = Join-Path ($ffmpegResolved ?? $ffmpegCandidates[0]) 'bin'

$sdkCandidates = @(
  $env:NV_RTX_VIDEO_SDK,
  (Join-Path $repoRoot 'RTX_Video_SDK_v1.1.0'),
  (Join-Path $repoRoot 'deps\RTX_Video_SDK_v1.1.0')
)
$sdkResolved = $sdkCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$sdkBin = Join-Path ($sdkResolved ?? $sdkCandidates[0]) 'bin\Windows\x64\rel'

$env:RTXHDR_NATIVE_DLL = $nativeDll
$env:PATH = "$ffmpegBin;$sdkBin;$env:PATH"

Push-Location (Join-Path $repoRoot 'app\flutter_app')
try {
  & $flutterExe run -d windows
}
finally {
  Pop-Location
}

