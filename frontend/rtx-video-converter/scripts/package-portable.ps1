param(
  [string]$ReleaseDir = "src-tauri\target\release"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tauri-packaging-common.ps1")

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ResolvedReleaseDir = Resolve-Path (Join-Path $ProjectRoot $ReleaseDir)
$PackageJsonPath = Join-Path $ProjectRoot "package.json"
$TauriConfigPath = Join-Path $ProjectRoot "src-tauri\tauri.conf.json"
$PackageJson = Get-Content -LiteralPath $PackageJsonPath -Raw | ConvertFrom-Json
$TauriConfig = Get-Content -LiteralPath $TauriConfigPath -Raw | ConvertFrom-Json

if (-not (Get-Command rustc -ErrorAction SilentlyContinue)) {
  throw "rustc is required to package the portable build because the sidecar file name depends on the host target triple."
}

$TargetTriple = (& rustc --print host-tuple).Trim()
if ([string]::IsNullOrWhiteSpace($TargetTriple)) {
  throw "rustc did not return a host target triple."
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

Get-PortablePayloadEntries -ProjectRoot $ProjectRoot -ResolvedReleaseDir $ResolvedReleaseDir -TargetTriple $TargetTriple | ForEach-Object {
  Copy-Item -LiteralPath $_.SourcePath -Destination (Join-Path $PortableDir $_.DestinationName) -Force
}

if (Test-Path -LiteralPath $ReadmeSource) {
  Copy-Item -LiteralPath $ReadmeSource -Destination $ReadmeDestination -Force
}

Compress-Archive -Path (Join-Path $PortableDir "*") -DestinationPath $PortableZip -CompressionLevel Optimal

Write-Host "Portable directory: $PortableDir"
Write-Host "Portable zip: $PortableZip"
