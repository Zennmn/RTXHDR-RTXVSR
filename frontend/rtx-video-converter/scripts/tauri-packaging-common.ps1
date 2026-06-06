function Get-BackendSearchRoots {
  param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
  )

  $roots = [System.Collections.Generic.List[string]]::new()

  $localWorkspaceRoot = Resolve-Path (Join-Path $ProjectRoot "..\..")
  $roots.Add($localWorkspaceRoot.Path)

  $ancestor = $localWorkspaceRoot.Path
  for ($i = 0; $i -lt 2; $i += 1) {
    $parent = Split-Path -Parent $ancestor
    if ([string]::IsNullOrWhiteSpace($parent) -or $roots -contains $parent) {
      break
    }
    $roots.Add($parent)
    $ancestor = $parent
  }

  try {
    $gitCommonDir = @(git -C $ProjectRoot rev-parse --git-common-dir 2>$null | Select-Object -First 1)
    if ($gitCommonDir.Count -gt 0 -and -not [string]::IsNullOrWhiteSpace($gitCommonDir[0])) {
      $commonDirPath = if ([System.IO.Path]::IsPathRooted($gitCommonDir[0])) {
        $gitCommonDir[0]
      } else {
        Join-Path $ProjectRoot $gitCommonDir[0]
      }
      $resolvedCommonDir = Resolve-Path -LiteralPath $commonDirPath -ErrorAction Stop
      $commonWorkspaceRoot = Split-Path -Parent $resolvedCommonDir.Path
      if ($roots -notcontains $commonWorkspaceRoot) {
        $roots.Add($commonWorkspaceRoot)
      }
    }
  } catch {
  }

  return @($roots)
}

function Resolve-BackendExecutablePath {
  param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [string]$BackendExe = ""
  )

  if (-not [string]::IsNullOrWhiteSpace($BackendExe)) {
    $candidate = if ([System.IO.Path]::IsPathRooted($BackendExe)) {
      $BackendExe
    } else {
      Join-Path $ProjectRoot $BackendExe
    }

    return (Resolve-Path -LiteralPath $candidate).Path
  }

  $candidateSuffixes = @(
    "build\backend-hw\Release\vsr_backend.exe",
    "build\backend\Release\vsr_backend.exe",
    "build\backend\Debug\vsr_backend.exe"
  )

  foreach ($suffix in $candidateSuffixes) {
    foreach ($root in Get-BackendSearchRoots -ProjectRoot $ProjectRoot) {
      $candidate = Join-Path $root $suffix
      if (Test-Path -LiteralPath $candidate) {
        return (Resolve-Path -LiteralPath $candidate).Path
      }
    }
  }

  throw "Could not find a backend sidecar executable. Looked for build\\backend-hw\\Release, build\\backend\\Release, and build\\backend\\Debug. Pass -BackendExe explicitly to override."
}

function Get-AdjacentRuntimeLibraryPaths {
  param(
    [Parameter(Mandatory = $true)]
    [string]$BackendDirectory
  )

  return @(Get-ChildItem -LiteralPath $BackendDirectory -Filter *.dll -File | Sort-Object Name | Select-Object -ExpandProperty FullName)
}

function Get-PortablePayloadEntries {
  param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [Parameter(Mandatory = $true)]
    [string]$ResolvedReleaseDir,

    [Parameter(Mandatory = $true)]
    [string]$TargetTriple
  )

  $entries = [ordered]@{}

  $appPath = Join-Path $ResolvedReleaseDir "rtx-video-converter.exe"
  if (-not (Test-Path -LiteralPath $appPath)) {
    throw "Portable packaging requires 'rtx-video-converter.exe' in '$ResolvedReleaseDir'. Run 'npm run tauri:build' first."
  }
  $entries["rtx-video-converter.exe"] = (Resolve-Path -LiteralPath $appPath).Path

  $sidecarPath = Join-Path $ProjectRoot "src-tauri\binaries\vsr_backend-$TargetTriple.exe"
  if (-not (Test-Path -LiteralPath $sidecarPath)) {
    $fallbackSidecarPath = Join-Path $ResolvedReleaseDir "vsr_backend.exe"
    if (-not (Test-Path -LiteralPath $fallbackSidecarPath)) {
      throw "Portable packaging could not find the prepared sidecar binary. Run 'npm run sidecar:prepare' first."
    }
    $sidecarPath = $fallbackSidecarPath
  }
  $entries["vsr_backend.exe"] = (Resolve-Path -LiteralPath $sidecarPath).Path

  foreach ($dll in Get-ChildItem -LiteralPath $ResolvedReleaseDir -Filter *.dll -File -ErrorAction SilentlyContinue) {
    $entries[$dll.Name] = $dll.FullName
  }

  $runtimeDir = Join-Path $ProjectRoot "src-tauri\runtime"
  foreach ($dll in Get-ChildItem -LiteralPath $runtimeDir -Filter *.dll -File -ErrorAction SilentlyContinue) {
    $entries[$dll.Name] = $dll.FullName
  }

  return @($entries.GetEnumerator() | ForEach-Object {
      [PSCustomObject]@{
        DestinationName = $_.Key
        SourcePath = $_.Value
      }
    })
}
