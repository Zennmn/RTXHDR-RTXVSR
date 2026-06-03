param(
  [string]$BackendExe = "..\..\build\backend\Debug\vsr_backend.exe"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BackendPath = Resolve-Path (Join-Path $ProjectRoot $BackendExe)
$BinariesDir = Join-Path $ProjectRoot "src-tauri\binaries"

if (-not (Get-Command rustc -ErrorAction SilentlyContinue)) {
  throw "rustc is required to prepare a Tauri sidecar binary. Install Rust, then rerun this script."
}

$TargetTriple = (& rustc --print host-tuple).Trim()
if ([string]::IsNullOrWhiteSpace($TargetTriple)) {
  throw "rustc did not return a host target triple."
}

New-Item -ItemType Directory -Force -Path $BinariesDir | Out-Null
$Destination = Join-Path $BinariesDir "vsr_backend-$TargetTriple.exe"
Copy-Item -LiteralPath $BackendPath -Destination $Destination -Force
Write-Host "Prepared sidecar: $Destination"
