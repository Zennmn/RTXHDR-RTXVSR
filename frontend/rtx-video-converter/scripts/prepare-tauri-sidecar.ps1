param(
  [string]$BackendExe = "..\..\build\backend-hw\Release\vsr_backend.exe"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BackendPath = Resolve-Path (Join-Path $ProjectRoot $BackendExe)
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
$Destination = Join-Path $BinariesDir "vsr_backend-$TargetTriple.exe"
Copy-Item -LiteralPath $BackendPath -Destination $Destination -Force

# Copy adjacent runtime DLLs so the packaged app can launch the sidecar with
# FFmpeg and RTX runtime dependencies present beside the final executable.
Get-ChildItem -LiteralPath $BackendDirectory -Filter *.dll | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $RuntimeDir $_.Name) -Force
}

$RequiredRuntimeDlls = @(
  "avcodec-62.dll",
  "avformat-62.dll",
  "avutil-60.dll",
  "swresample-6.dll",
  "swscale-9.dll",
  "nvngx_vsr.dll",
  "nvngx_truehdr.dll"
)

foreach ($RequiredRuntimeDll in $RequiredRuntimeDlls) {
  $Candidate = Join-Path $RuntimeDir $RequiredRuntimeDll
  if (-not (Test-Path -LiteralPath $Candidate)) {
    throw "Required runtime DLL missing after sidecar preparation: $RequiredRuntimeDll"
  }
}

Write-Host "Prepared sidecar: $Destination"
