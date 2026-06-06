param(
  [string]$BackendExe = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tauri-packaging-common.ps1")

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BackendPath = Resolve-BackendExecutablePath -ProjectRoot $ProjectRoot -BackendExe $BackendExe
$BackendDirectory = Split-Path -Parent $BackendPath
$BinariesDir = Join-Path $ProjectRoot "src-tauri\binaries"
$RuntimeDir = Join-Path $ProjectRoot "src-tauri\runtime"

if (-not (Get-Command rustc -ErrorAction SilentlyContinue)) {
  throw "rustc is required to prepare a Tauri sidecar binary. Install Rust, then rerun this script."
}

$TargetTriple = (& rustc --print host-tuple).Trim()
if ([string]::IsNullOrWhiteSpace($TargetTriple)) {
  throw "rustc did not return a host target triple."
}

New-Item -ItemType Directory -Force -Path $BinariesDir | Out-Null
New-Item -ItemType Directory -Force -Path $RuntimeDir | Out-Null
Get-ChildItem -LiteralPath $BinariesDir -Filter "vsr_backend-*.exe" -File -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $BinariesDir -Filter *.dll -File -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $RuntimeDir -Filter *.dll -File -ErrorAction SilentlyContinue | Remove-Item -Force

$Destination = Join-Path $BinariesDir "vsr_backend-$TargetTriple.exe"
Copy-Item -LiteralPath $BackendPath -Destination $Destination -Force

# Copy adjacent runtime DLLs so the packaged app can launch the sidecar with
# FFmpeg and RTX runtime dependencies present beside the sidecar during dev,
# and inside the packaged app resources for installer/portable builds.
Get-AdjacentRuntimeLibraryPaths -BackendDirectory $BackendDirectory | ForEach-Object {
  $runtimeLibraryPath = $_
  $fileName = Split-Path -Leaf $runtimeLibraryPath
  Copy-Item -LiteralPath $runtimeLibraryPath -Destination (Join-Path $BinariesDir $fileName) -Force
  Copy-Item -LiteralPath $runtimeLibraryPath -Destination (Join-Path $RuntimeDir $fileName) -Force
}

Write-Host "Prepared sidecar: $Destination"
Write-Host "Backend source: $BackendPath"
