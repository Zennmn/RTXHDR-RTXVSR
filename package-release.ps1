param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = "1.0.0",
    [string]$PublishDirectory = "",
    [string]$FfmpegArtifactRoot = "",
    [string]$ThirdPartyNoticesDirectory = "",
    [string]$ThirdPartyBackendBuildDirectory = "",
    [string]$InnoCompiler = ""
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

if ([string]::IsNullOrWhiteSpace($PublishDirectory)) {
    $PublishDirectory = Join-Path $root "frontend\rtx-video-converter-winui\bin\publish\win-x64"
}
$PublishDirectory = [System.IO.Path]::GetFullPath($PublishDirectory)
if ([string]::IsNullOrWhiteSpace($FfmpegArtifactRoot)) {
    $FfmpegArtifactRoot = Join-Path $root "artifacts\ffmpeg-minimal"
}
$FfmpegArtifactRoot = [System.IO.Path]::GetFullPath($FfmpegArtifactRoot)
if ([string]::IsNullOrWhiteSpace($ThirdPartyNoticesDirectory)) {
    $ThirdPartyNoticesDirectory = Join-Path $root "artifacts\third-party-notices"
}
$ThirdPartyNoticesDirectory = [System.IO.Path]::GetFullPath($ThirdPartyNoticesDirectory)

$noticeArguments = @{
    OutputDirectory = $ThirdPartyNoticesDirectory
    ProjectAssetsFile = Join-Path $root "frontend\rtx-video-converter-winui\obj\project.assets.json"
    PublishDirectory = $PublishDirectory
}
if (-not [string]::IsNullOrWhiteSpace($ThirdPartyBackendBuildDirectory)) {
    $noticeArguments.BackendBuildDirectory = $ThirdPartyBackendBuildDirectory
}
& (Join-Path $root "generate-third-party-notices.ps1") @noticeArguments

& (Join-Path $root "verify-release-compliance.ps1") `
    -PublishDirectory $PublishDirectory `
    -FfmpegArtifactRoot $FfmpegArtifactRoot `
    -ThirdPartyNoticesDirectory $ThirdPartyNoticesDirectory `
    -Version $Version

$requiredPublishFiles = @(
    (Join-Path $PublishDirectory "RTXVideoConverter.WinUI.exe"),
    (Join-Path $PublishDirectory "RTXVideoConverter.WinUI.pri"),
    (Join-Path $PublishDirectory "runtime\vsr_backend.exe"),
    (Join-Path $PublishDirectory "runtime\nvngx_vsr.dll"),
    (Join-Path $PublishDirectory "runtime\nvngx_truehdr.dll")
)
foreach ($required in $requiredPublishFiles) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required published file is missing: $required"
    }
}
if (-not (Get-ChildItem -LiteralPath (Join-Path $PublishDirectory "runtime") -Filter "avcodec-*.dll" -File)) {
    throw "The published runtime does not contain an FFmpeg avcodec DLL."
}

$stageRoot = Assert-WorkspacePath (Join-Path $root "artifacts\release-v$Version")
$payloadDirectory = Join-Path $stageRoot "RTX Video Converter"
$releaseDirectory = Assert-WorkspacePath (Join-Path $root "dist\v$Version")
foreach ($target in @($stageRoot, $releaseDirectory)) {
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $target | Out-Null
}
New-Item -ItemType Directory -Force -Path $payloadDirectory | Out-Null

Copy-Item -Path (Join-Path $PublishDirectory "*") -Destination $payloadDirectory -Recurse -Force
Get-ChildItem -LiteralPath $payloadDirectory -Recurse -File -Filter "*.pdb" | Remove-Item -Force
foreach ($unwanted in @("qa", "startup-error.txt")) {
    $path = Join-Path $payloadDirectory $unwanted
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

Copy-Item -LiteralPath (Join-Path $root "LICENSE") -Destination (Join-Path $payloadDirectory "LICENSE.txt")
Copy-Item -LiteralPath (Join-Path $root "DISTRIBUTION_TERMS.txt") -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $root "THIRD_PARTY.md") -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $ThirdPartyNoticesDirectory "THIRD_PARTY_NOTICES.txt") -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $ThirdPartyNoticesDirectory "THIRD_PARTY_LICENSES") -Destination $payloadDirectory -Recurse
Copy-Item -LiteralPath (Join-Path $FfmpegArtifactRoot "runtime\FFMPEG_LICENSE_LGPLv2.1.txt") -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $FfmpegArtifactRoot "runtime\NV_CODEC_HEADERS_LICENSE.txt") -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $root "RTX_Video_SDK_v1.1.0\NVIDIA_RTX_Video_SDK_License.pdf") -Destination $payloadDirectory
$portableReadme = (Get-Content -Raw (Join-Path $root "packaging\README-portable.txt")).Replace("{{VERSION}}", $Version)
[System.IO.File]::WriteAllText(
    (Join-Path $payloadDirectory "README-portable.txt"),
    $portableReadme,
    [System.Text.UTF8Encoding]::new($false))

$portableZip = Join-Path $releaseDirectory "RTX.Video.Converter_${Version}_x64-portable.zip"
Compress-Archive -Path (Join-Path $payloadDirectory "*") -DestinationPath $portableZip -CompressionLevel Optimal

$sourcePayload = Join-Path $stageRoot "FFmpeg corresponding source"
New-Item -ItemType Directory -Force -Path $sourcePayload | Out-Null
Copy-Item -Path (Join-Path $FfmpegArtifactRoot "sources\*") -Destination $sourcePayload -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root "third_party\ffmpeg\Dockerfile") -Destination $sourcePayload
Copy-Item -LiteralPath (Join-Path $root "third_party\ffmpeg\README.md") -Destination $sourcePayload
Copy-Item -LiteralPath (Join-Path $root "third_party\ffmpeg\NV_CODEC_HEADERS_LICENSE.txt") -Destination $sourcePayload
Copy-Item -LiteralPath (Join-Path $root "build-minimal-ffmpeg.ps1") -Destination $sourcePayload
$sourceZip = Join-Path $releaseDirectory "RTX.Video.Converter_${Version}_FFmpeg-corresponding-source.zip"
Compress-Archive -Path (Join-Path $sourcePayload "*") -DestinationPath $sourceZip -CompressionLevel Optimal

if ([string]::IsNullOrWhiteSpace($InnoCompiler)) {
    $compilerCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )
    $InnoCompiler = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($InnoCompiler) -or -not (Test-Path -LiteralPath $InnoCompiler -PathType Leaf)) {
    throw "Inno Setup Compiler (ISCC.exe) was not found."
}

$installerScript = Join-Path $root "packaging\RTXVideoConverter.iss"
& $InnoCompiler "/DAppVersion=$Version" "/DSourceDir=$payloadDirectory" "/DOutputDir=$releaseDirectory" $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$installer = Join-Path $releaseDirectory "RTX.Video.Converter_${Version}_x64-setup.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Installer output was not created: $installer"
}

$artifacts = @($portableZip, $installer, $sourceZip)
$hashLines = foreach ($artifact in $artifacts) {
    $hash = Get-FileHash -LiteralPath $artifact -Algorithm SHA256
    "$($hash.Hash)  $([System.IO.Path]::GetFileName($artifact))"
}
$hashFile = Join-Path $releaseDirectory "SHA256SUMS.txt"
[System.IO.File]::WriteAllLines($hashFile, $hashLines, [System.Text.UTF8Encoding]::new($false))

$artifacts + $hashFile | ForEach-Object {
    $item = Get-Item -LiteralPath $_
    [pscustomobject]@{
        Path = $item.FullName
        Bytes = $item.Length
        SHA256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}
