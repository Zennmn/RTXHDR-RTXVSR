param(
    [string]$ImageName = "rtx-video-converter-ffmpeg:1.0.0"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

function Assert-WorkspacePath([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $rootPrefix = [System.IO.Path]::GetFullPath($root) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the workspace: $full"
    }
    return $full
}

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker is required to build the pinned FFmpeg runtime."
}
docker version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Docker is installed but its daemon is not available."
}

$context = Join-Path $root "third_party\ffmpeg"
$output = Assert-WorkspacePath (Join-Path $root "artifacts\ffmpeg-minimal")
if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $output | Out-Null

docker build --pull -t $ImageName $context
if ($LASTEXITCODE -ne 0) {
    throw "Minimal FFmpeg Docker build failed."
}

$container = docker create $ImageName
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($container)) {
    throw "Could not create the FFmpeg export container."
}
try {
    docker cp "${container}:/export/." $output
    if ($LASTEXITCODE -ne 0) {
        throw "Could not export the FFmpeg build artifacts."
    }
}
finally {
    docker rm $container | Out-Null
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "Visual Studio locator was not found: $vswhere"
}
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($visualStudio)) {
    throw "Visual Studio C++ tools are required to generate MSVC import libraries."
}
$libExe = Get-ChildItem -LiteralPath (Join-Path $visualStudio "VC\Tools\MSVC") -Recurse -Filter lib.exe -File |
    Where-Object { $_.FullName -match '\\bin\\Hostx64\\x64\\lib\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if ($null -eq $libExe) {
    throw "The x64 MSVC library manager (lib.exe) was not found."
}
$developmentLib = Join-Path $output "development\lib"
Get-ChildItem -LiteralPath $developmentLib -Filter "*.def" -File | ForEach-Object {
    $baseName = $_.BaseName -replace '-\d+$', ''
    $importLibrary = Join-Path $developmentLib "$baseName.lib"
    & $libExe.FullName /nologo "/def:$($_.FullName)" "/out:$importLibrary" /machine:x64
    if ($LASTEXITCODE -ne 0) {
        throw "Could not generate the MSVC import library: $importLibrary"
    }
}

$sdkRoot = Join-Path $output "sdk"
New-Item -ItemType Directory -Force -Path (Join-Path $sdkRoot "bin") | Out-Null
Copy-Item -LiteralPath (Join-Path $output "development\include") -Destination $sdkRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $output "development\lib") -Destination $sdkRoot -Recurse -Force
Copy-Item -Path (Join-Path $output "runtime\*.dll") -Destination (Join-Path $sdkRoot "bin") -Force

$required = @(
    "runtime\avcodec-63.dll",
    "runtime\avformat-63.dll",
    "runtime\avutil-61.dll",
    "runtime\swresample-7.dll",
    "runtime\swscale-10.dll",
    "development\include\libavcodec\avcodec.h",
    "development\lib\avcodec.lib",
    "sdk\bin\avcodec-63.dll",
    "sdk\include\libavcodec\avcodec.h",
    "sdk\lib\avcodec.lib",
    "sources\FFmpeg-a09be9b91e8e1219f297586873b0d7322b47df96-source.tar.xz",
    "sources\nv-codec-headers-15ee32753c92faddbabbff11676779618fc6db7e-source.tar.xz",
    "sources\ffmpeg-config.log"
)
foreach ($relative in $required) {
    $path = Join-Path $output $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Expected FFmpeg build output is missing: $path"
    }
}

Get-ChildItem -LiteralPath $output -Recurse -File | ForEach-Object {
    [pscustomobject]@{
        Path = $_.FullName
        Bytes = $_.Length
        SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
}
