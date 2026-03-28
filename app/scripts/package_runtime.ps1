[CmdletBinding()]
param(
  [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputRoot = Join-Path $repoRoot 'dist'
$bundleName = 'RTXHDR_RTXVSR_Windows_Portable'
$bundleRoot = Join-Path $outputRoot $bundleName
$bundleZip = Join-Path $outputRoot "$bundleName.zip"
$flutterRoot = if ($env:FLUTTER_ROOT) { $env:FLUTTER_ROOT } else { 'C:\Users\21826\develop\flutter' }
$flutterExe = Join-Path $flutterRoot 'bin\flutter.bat'
$releaseRoot = Join-Path $repoRoot 'app\flutter_app\build\windows\x64\runner\Release'

if (-not (Test-Path $flutterExe)) {
  throw "Unable to locate flutter.bat at $flutterExe"
}

pwsh "$PSScriptRoot\configure_native.ps1" -Build -Configuration $Configuration | Out-Host

Push-Location (Join-Path $repoRoot 'app\flutter_app')
try {
  & $flutterExe build windows --release
}
finally {
  Pop-Location
}

if (-not (Test-Path $releaseRoot)) {
  throw "Flutter release output not found at $releaseRoot"
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path $bundleRoot) {
  Remove-Item -Recurse -Force $bundleRoot
}
New-Item -ItemType Directory -Force -Path $bundleRoot | Out-Null

Copy-Item -Path (Join-Path $releaseRoot '*') -Destination $bundleRoot -Recurse -Force

if (Test-Path $bundleZip) {
  Remove-Item -Force $bundleZip
}
Compress-Archive -Path (Join-Path $bundleRoot '*') -DestinationPath $bundleZip -CompressionLevel Optimal

Write-Host "Portable bundle: $bundleRoot"
Write-Host "Portable zip: $bundleZip"
