param(
    [Parameter(Mandatory = $true)]
    [string]$PublishDirectory,
    [string]$FfmpegArtifactRoot = "",
    [string]$ThirdPartyNoticesDirectory = "",
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$ffmpegCommit = "a09be9b91e8e1219f297586873b0d7322b47df96"
$nvCodecHeadersCommit = "15ee32753c92faddbabbff11676779618fc6db7e"

if ([string]::IsNullOrWhiteSpace($FfmpegArtifactRoot)) {
    $FfmpegArtifactRoot = Join-Path $root "artifacts\ffmpeg-minimal"
}
if ([string]::IsNullOrWhiteSpace($ThirdPartyNoticesDirectory)) {
    $ThirdPartyNoticesDirectory = Join-Path $root "artifacts\third-party-notices"
}
$PublishDirectory = [System.IO.Path]::GetFullPath($PublishDirectory)
$FfmpegArtifactRoot = [System.IO.Path]::GetFullPath($FfmpegArtifactRoot)
$ThirdPartyNoticesDirectory = [System.IO.Path]::GetFullPath($ThirdPartyNoticesDirectory)

function Assert-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required compliance file is missing: $Path"
    }
}

foreach ($relative in @(
    "DISTRIBUTION_TERMS.txt",
    "THIRD_PARTY.md",
    "LICENSE",
    "docs\NVIDIA_RELEASE_CHECKLIST.md",
    "RTX_Video_SDK_v1.1.0\NVIDIA_RTX_Video_SDK_License.pdf"
)) {
    Assert-File (Join-Path $root $relative)
}

$sourceDirectory = Join-Path $FfmpegArtifactRoot "sources"
$runtimeDirectory = Join-Path $FfmpegArtifactRoot "runtime"
$configLog = Join-Path $sourceDirectory "ffmpeg-config.log"
foreach ($relative in @(
    "FFmpeg-$ffmpegCommit-source.tar.xz",
    "nv-codec-headers-$nvCodecHeadersCommit-source.tar.xz",
    "ffmpeg-config.log",
    "BUILD-AND-SOURCE-NOTICE.md"
)) {
    Assert-File (Join-Path $sourceDirectory $relative)
}
foreach ($license in @("FFMPEG_LICENSE_LGPLv2.1.txt", "NV_CODEC_HEADERS_LICENSE.txt")) {
    Assert-File (Join-Path $runtimeDirectory $license)
}

$thirdPartyIndex = Join-Path $ThirdPartyNoticesDirectory "THIRD_PARTY_NOTICES.txt"
Assert-File $thirdPartyIndex
$thirdPartyIndexText = Get-Content -LiteralPath $thirdPartyIndex -Raw
foreach ($component in @(
    "cpp-httplib 0.15.3",
    "nlohmann/json 3.11.3",
    "CommunityToolkit.Mvvm/8.4.0",
    "Microsoft.Extensions.DependencyInjection/8.0.1",
    "Microsoft.Extensions.DependencyInjection.Abstractions/8.0.2",
    "Microsoft.Web.WebView2/1.0.3719.77",
    "Microsoft.Windows.SDK.BuildTools/10.0.26100.4654",
    "Microsoft.WindowsAppSDK/2.2.0",
    "System.Numerics.Tensors/9.0.0",
    ".NET Runtime 8.0.28"
)) {
    if ($thirdPartyIndexText.IndexOf($component, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Third-party notice index does not cover required component: $component"
    }
}

$indexLines = Get-Content -LiteralPath $thirdPartyIndex
$verifiedNoticeFiles = 0
for ($index = 0; $index -lt $indexLines.Count; $index++) {
    if (-not $indexLines[$index].StartsWith("File: ", [System.StringComparison]::Ordinal)) {
        continue
    }
    if ($index + 1 -ge $indexLines.Count -or
        -not $indexLines[$index + 1].StartsWith("SHA256: ", [System.StringComparison]::Ordinal)) {
        throw "Third-party notice index has an invalid file/hash pair at line $($index + 1)."
    }
    $relativePath = $indexLines[$index].Substring(6).Replace("/", [System.IO.Path]::DirectorySeparatorChar)
    $noticePath = Join-Path $ThirdPartyNoticesDirectory $relativePath
    Assert-File $noticePath
    $expectedHash = $indexLines[$index + 1].Substring(8).Trim()
    $actualHash = (Get-FileHash -LiteralPath $noticePath -Algorithm SHA256).Hash
    if ($actualHash -ne $expectedHash) {
        throw "Third-party notice hash mismatch: $noticePath"
    }
    $verifiedNoticeFiles++
}
if ($verifiedNoticeFiles -lt 30) {
    throw "Third-party notice inventory is unexpectedly small: $verifiedNoticeFiles files"
}

$configText = Get-Content -LiteralPath $configLog -Raw
foreach ($requiredFlag in @("--enable-shared", "--disable-static", "--disable-autodetect", "--enable-protocol=file")) {
    if ($configText.IndexOf($requiredFlag, [System.StringComparison]::Ordinal) -lt 0) {
        throw "FFmpeg configure log does not contain required flag: $requiredFlag"
    }
}
foreach ($forbiddenFlag in @("--enable-gpl", "--enable-nonfree", "--enable-libx264", "--enable-libx265")) {
    if ($configText.IndexOf($forbiddenFlag, [System.StringComparison]::Ordinal) -ge 0) {
        throw "FFmpeg configure log contains forbidden release flag: $forbiddenFlag"
    }
}

$publishRuntime = Join-Path $PublishDirectory "runtime"
foreach ($sourceDll in Get-ChildItem -LiteralPath $runtimeDirectory -Filter "*.dll" -File) {
    $publishedDll = Join-Path $publishRuntime $sourceDll.Name
    Assert-File $publishedDll
    $sourceHash = (Get-FileHash -LiteralPath $sourceDll.FullName -Algorithm SHA256).Hash
    $publishedHash = (Get-FileHash -LiteralPath $publishedDll -Algorithm SHA256).Hash
    if ($sourceHash -ne $publishedHash) {
        throw "Published FFmpeg DLL does not match the pinned build: $($sourceDll.Name)"
    }
}

$unexpectedFfmpegDlls = Get-ChildItem -LiteralPath $publishRuntime -File |
    Where-Object { $_.Name -match '^(avcodec|avdevice|avfilter|avformat|avutil|swresample|swscale)-\d+\.dll$' } |
    Where-Object { -not (Test-Path -LiteralPath (Join-Path $runtimeDirectory $_.Name) -PathType Leaf) }
if ($unexpectedFfmpegDlls) {
    throw "Published runtime contains FFmpeg DLLs outside the pinned build: $($unexpectedFfmpegDlls.Name -join ', ')"
}

[pscustomobject]@{
    Version = $Version
    FfmpegCommit = $ffmpegCommit
    NvCodecHeadersCommit = $nvCodecHeadersCommit
    PublishedFfmpegDlls = (Get-ChildItem -LiteralPath $runtimeDirectory -Filter "*.dll" -File).Count
    ThirdPartyNoticeFiles = $verifiedNoticeFiles
    Status = "compliance checks passed"
}
