param(
  [string]$ReleaseDir = "src-tauri\target\release"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ResolvedReleaseDir = Resolve-Path (Join-Path $ProjectRoot $ReleaseDir)
$PackageJsonPath = Join-Path $ProjectRoot "package.json"
$TauriConfigPath = Join-Path $ProjectRoot "src-tauri\tauri.conf.json"
$PackageJson = Get-Content -LiteralPath $PackageJsonPath -Raw | ConvertFrom-Json
$TauriConfig = Get-Content -LiteralPath $TauriConfigPath -Raw | ConvertFrom-Json

$AppName = "rtx-video-converter.exe"
$BackendName = "vsr_backend.exe"
$RequiredFiles = @($AppName, $BackendName)

foreach ($RequiredFile in $RequiredFiles) {
  $Candidate = Join-Path $ResolvedReleaseDir $RequiredFile
  if (-not (Test-Path -LiteralPath $Candidate)) {
    throw "Portable packaging requires '$RequiredFile' in '$ResolvedReleaseDir'. Run 'npm run tauri:build' first."
  }
}

$ProductName = [string]$TauriConfig.productName
if ([string]::IsNullOrWhiteSpace($ProductName)) {
  $ProductName = [string]$PackageJson.name
}
$PortableLabel = $ProductName.Replace("/", " ").Trim()
$Version = [string]$PackageJson.version
if ([string]::IsNullOrWhiteSpace($Version)) {
  $Version = "0.0.0"
}

$BundleRoot = Join-Path $ResolvedReleaseDir "bundle"
$PortableRoot = Join-Path $BundleRoot "portable"
$PortableDirName = "$PortableLabel" + "_" + $Version + "_x64-portable"
$PortableDir = Join-Path $PortableRoot $PortableDirName
$PortableZip = Join-Path $PortableRoot ($PortableDirName + ".zip")
$ReadmeSource = Join-Path $ProjectRoot "assets\portable\README-portable.txt"
$ReadmeDestination = Join-Path $PortableDir "README-portable.txt"

New-Item -ItemType Directory -Force -Path $PortableRoot | Out-Null
if (Test-Path -LiteralPath $PortableDir) {
  Remove-Item -LiteralPath $PortableDir -Recurse -Force
}
if (Test-Path -LiteralPath $PortableZip) {
  Remove-Item -LiteralPath $PortableZip -Force
}
New-Item -ItemType Directory -Force -Path $PortableDir | Out-Null

Get-ChildItem -LiteralPath $ResolvedReleaseDir -File | Where-Object {
  $_.Name -eq $AppName -or
  $_.Name -eq $BackendName -or
  $_.Extension -eq ".dll"
} | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $PortableDir $_.Name) -Force
}

if (Test-Path -LiteralPath $ReadmeSource) {
  Copy-Item -LiteralPath $ReadmeSource -Destination $ReadmeDestination -Force
}

Compress-Archive -Path (Join-Path $PortableDir "*") -DestinationPath $PortableZip -CompressionLevel Optimal

Write-Host "Portable directory: $PortableDir"
Write-Host "Portable zip: $PortableZip"
