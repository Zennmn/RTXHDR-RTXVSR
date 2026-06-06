$HelperPath = Join-Path $PSScriptRoot "..\tauri-packaging-common.ps1"
if (Test-Path -LiteralPath $HelperPath) {
  . $HelperPath
}

Describe "Resolve-BackendExecutablePath" {
  It "prefers the hardware backend release build by default" {
    $workspaceRoot = Join-Path $TestDrive "workspace"
    $projectRoot = Join-Path $workspaceRoot "frontend\rtx-video-converter"
    $hardwareBackend = Join-Path $workspaceRoot "build\backend-hw\Release\vsr_backend.exe"
    $fallbackBackend = Join-Path $workspaceRoot "build\backend\Release\vsr_backend.exe"

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $hardwareBackend) | Out-Null
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fallbackBackend) | Out-Null
    New-Item -ItemType Directory -Force -Path $projectRoot | Out-Null
    Set-Content -LiteralPath $hardwareBackend -Value "hardware"
    Set-Content -LiteralPath $fallbackBackend -Value "fallback"

    $resolved = Resolve-BackendExecutablePath -ProjectRoot $projectRoot

    $resolved | Should Be (Resolve-Path -LiteralPath $hardwareBackend).Path
  }

  It "finds the hardware backend from the git common root when running in a worktree" {
    $workspaceRoot = Join-Path $TestDrive "workspace"
    $worktreeRoot = Join-Path $workspaceRoot ".worktrees\codex-vsr-bug-fixes"
    $projectRoot = Join-Path $worktreeRoot "frontend\rtx-video-converter"
    $hardwareBackend = Join-Path $workspaceRoot "build\backend-hw\Release\vsr_backend.exe"
    $fallbackBackend = Join-Path $worktreeRoot "build\backend\Release\vsr_backend.exe"

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $hardwareBackend) | Out-Null
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fallbackBackend) | Out-Null
    New-Item -ItemType Directory -Force -Path $projectRoot | Out-Null
    Set-Content -LiteralPath $hardwareBackend -Value "hardware"
    Set-Content -LiteralPath $fallbackBackend -Value "fallback"

    $resolved = Resolve-BackendExecutablePath -ProjectRoot $projectRoot

    $resolved | Should Be (Resolve-Path -LiteralPath $hardwareBackend).Path
  }
}

Describe "Get-PortablePayloadEntries" {
  It "includes runtime DLLs together with the app and sidecar" {
    $workspaceRoot = Join-Path $TestDrive "workspace"
    $projectRoot = Join-Path $workspaceRoot "frontend\rtx-video-converter"
    $releaseDir = Join-Path $projectRoot "src-tauri\target\release"
    $binariesDir = Join-Path $projectRoot "src-tauri\binaries"
    $runtimeDir = Join-Path $projectRoot "src-tauri\runtime"

    New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
    New-Item -ItemType Directory -Force -Path $binariesDir | Out-Null
    New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null

    Set-Content -LiteralPath (Join-Path $releaseDir "rtx-video-converter.exe") -Value "app"
    Set-Content -LiteralPath (Join-Path $binariesDir "vsr_backend-x86_64-pc-windows-msvc.exe") -Value "sidecar"
    Set-Content -LiteralPath (Join-Path $runtimeDir "nvngx_vsr.dll") -Value "vsr"
    Set-Content -LiteralPath (Join-Path $runtimeDir "avcodec-62.dll") -Value "codec"

    $entries = Get-PortablePayloadEntries `
      -ProjectRoot $projectRoot `
      -ResolvedReleaseDir (Resolve-Path -LiteralPath $releaseDir).Path `
      -TargetTriple "x86_64-pc-windows-msvc"

    $names = @($entries | Select-Object -ExpandProperty DestinationName)
    ($names -contains "rtx-video-converter.exe") | Should Be $true
    ($names -contains "vsr_backend.exe") | Should Be $true
    ($names -contains "nvngx_vsr.dll") | Should Be $true
    ($names -contains "avcodec-62.dll") | Should Be $true
  }
}
