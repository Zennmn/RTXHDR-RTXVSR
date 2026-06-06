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

$ValidationSessionId = "11111111-2222-3333-4444-555555555555"
$ValidationPort = Get-Random -Minimum 49152 -Maximum 65500
$ValidationProcess = $null
try {
  $ValidationProcess = Start-Process -FilePath $BackendPath -ArgumentList @(
    "--port",
    [string]$ValidationPort,
    "--app-session-id",
    $ValidationSessionId
  ) -PassThru -WindowStyle Hidden

  $Health = $null
  for ($Attempt = 0; $Attempt -lt 20; $Attempt++) {
    Start-Sleep -Milliseconds 250
    try {
      $Health = Invoke-RestMethod -Uri "http://127.0.0.1:$ValidationPort/api/health" -TimeoutSec 1
      break
    } catch {
      if ($ValidationProcess.HasExited) {
        break
      }
    }
  }

  if ($null -eq $Health) {
    throw "Backend sidecar validation failed: health endpoint did not respond."
  }
  if ($Health.appSessionId -ne $ValidationSessionId) {
    throw "Backend sidecar validation failed: /api/health did not echo --app-session-id. Rebuild the backend before packaging."
  }
} finally {
  if ($null -ne $ValidationProcess -and -not $ValidationProcess.HasExited) {
    Stop-Process -Id $ValidationProcess.Id -Force -ErrorAction SilentlyContinue
  }
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
