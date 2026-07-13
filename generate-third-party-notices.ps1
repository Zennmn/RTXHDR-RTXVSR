param(
    [string]$OutputDirectory = "",
    [string]$ProjectAssetsFile = "",
    [string]$PublishDirectory = "",
    [string]$BackendBuildDirectory = "",
    [string]$NuGetPackagesDirectory = "",
    [string]$DotnetRoot = "",
    [string]$WindowsSdkLicenseDirectory = ""
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

function Assert-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required third-party notice source is missing: $Path"
    }
}

function Copy-NoticeFile(
    [System.Collections.Generic.List[object]]$Entries,
    [string]$Component,
    [string]$SourcePath,
    [string]$DestinationName) {
    Assert-File $SourcePath
    $destination = Join-Path $licensesDirectory $DestinationName
    Copy-Item -LiteralPath $SourcePath -Destination $destination -Force
    $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    $Entries.Add([pscustomobject]@{
        Component = $Component
        File = "THIRD_PARTY_LICENSES/$DestinationName"
        SHA256 = $hash
    })
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root "artifacts\third-party-notices"
}
if ([string]::IsNullOrWhiteSpace($ProjectAssetsFile)) {
    $ProjectAssetsFile = Join-Path $root "frontend\rtx-video-converter-winui\obj\project.assets.json"
}
if ([string]::IsNullOrWhiteSpace($PublishDirectory)) {
    $PublishDirectory = Join-Path $root "frontend\rtx-video-converter-winui\bin\publish\win-x64"
}
if ([string]::IsNullOrWhiteSpace($NuGetPackagesDirectory)) {
    $NuGetPackagesDirectory = if ([string]::IsNullOrWhiteSpace($env:NUGET_PACKAGES)) {
        Join-Path $env:USERPROFILE ".nuget\packages"
    } else {
        $env:NUGET_PACKAGES
    }
}
if ([string]::IsNullOrWhiteSpace($DotnetRoot)) {
    $dotnet = Get-Command dotnet -ErrorAction Stop
    $DotnetRoot = Split-Path -Parent $dotnet.Source
}
if ([string]::IsNullOrWhiteSpace($WindowsSdkLicenseDirectory)) {
    $WindowsSdkLicenseDirectory = "C:\Program Files (x86)\Windows Kits\10\Licenses\10.0.26100.0"
}
if ([string]::IsNullOrWhiteSpace($BackendBuildDirectory)) {
    $BackendBuildDirectory = @(
        (Join-Path $root "build\backend-compliance"),
        (Join-Path $root "build\backend-hw"),
        (Join-Path $root "build\backend")
    ) | Where-Object {
        Test-Path -LiteralPath (Join-Path $_ "_deps\httplib-src\LICENSE") -PathType Leaf
    } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($BackendBuildDirectory)) {
    throw "A configured backend build containing cpp-httplib and nlohmann/json is required."
}

$OutputDirectory = Assert-WorkspacePath $OutputDirectory
$ProjectAssetsFile = [System.IO.Path]::GetFullPath($ProjectAssetsFile)
$PublishDirectory = [System.IO.Path]::GetFullPath($PublishDirectory)
$BackendBuildDirectory = [System.IO.Path]::GetFullPath($BackendBuildDirectory)
$NuGetPackagesDirectory = [System.IO.Path]::GetFullPath($NuGetPackagesDirectory)
$DotnetRoot = [System.IO.Path]::GetFullPath($DotnetRoot)
$WindowsSdkLicenseDirectory = [System.IO.Path]::GetFullPath($WindowsSdkLicenseDirectory)

Assert-File $ProjectAssetsFile
Assert-File (Join-Path $PublishDirectory "System.Private.CoreLib.dll")

if (Test-Path -LiteralPath $OutputDirectory) {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$licensesDirectory = Join-Path $OutputDirectory "THIRD_PARTY_LICENSES"
New-Item -ItemType Directory -Force -Path $licensesDirectory | Out-Null

$entries = [System.Collections.Generic.List[object]]::new()
Copy-NoticeFile $entries "cpp-httplib 0.15.3" `
    (Join-Path $BackendBuildDirectory "_deps\httplib-src\LICENSE") `
    "cpp-httplib-0.15.3-LICENSE.txt"
Copy-NoticeFile $entries "nlohmann/json 3.11.3" `
    (Join-Path $BackendBuildDirectory "_deps\nlohmann_json-src\LICENSE.MIT") `
    "nlohmann-json-3.11.3-LICENSE.MIT.txt"

$assets = Get-Content -LiteralPath $ProjectAssetsFile -Raw | ConvertFrom-Json
$packageLibraries = $assets.libraries.PSObject.Properties |
    Where-Object { $_.Value.type -eq "package" } |
    Sort-Object Name

foreach ($library in $packageLibraries) {
    $packageId, $version = $library.Name -split "/", 2
    $packageDirectory = Join-Path $NuGetPackagesDirectory (Join-Path $packageId.ToLowerInvariant() $version)
    if (-not (Test-Path -LiteralPath $packageDirectory -PathType Container)) {
        throw "Restored NuGet package is missing: $($library.Name)"
    }

    $noticeSources = Get-ChildItem -LiteralPath $packageDirectory -Recurse -File |
        Where-Object {
            $_.Name -match "(?i)^(license|notice|third[-_ ]?party[-_ ]?notices|sdk_license)(\..+)?$"
        } |
        Sort-Object Name

    if (-not $noticeSources -and $packageId -ne "Microsoft.Windows.SDK.BuildTools") {
        throw "No package license or notice file was found for $($library.Name)."
    }

    foreach ($source in $noticeSources) {
        $safePackage = $packageId -replace "[^A-Za-z0-9._-]", "-"
        $relativeSource = $source.FullName.Substring($packageDirectory.Length + 1)
        $safeSource = $relativeSource -replace "[\\/]", "__"
        $destinationName = "$safePackage-$version-$safeSource"
        Copy-NoticeFile $entries $library.Name $source.FullName $destinationName
    }
}

$dotnetVersion = (Get-Item -LiteralPath (Join-Path $PublishDirectory "System.Private.CoreLib.dll")).VersionInfo.ProductVersion
$dotnetVersion = ($dotnetVersion -split "\+", 2)[0]
Copy-NoticeFile $entries ".NET Runtime $dotnetVersion" `
    (Join-Path $DotnetRoot "LICENSE.txt") ".NET-Runtime-$dotnetVersion-LICENSE.txt"
Copy-NoticeFile $entries ".NET Runtime $dotnetVersion" `
    (Join-Path $DotnetRoot "ThirdPartyNotices.txt") ".NET-Runtime-$dotnetVersion-ThirdPartyNotices.txt"

$buildTools = $packageLibraries | Where-Object { $_.Name -like "Microsoft.Windows.SDK.BuildTools/*" } |
    Select-Object -First 1
$windowsSdkComponent = if ($null -eq $buildTools) {
    "Microsoft Windows SDK 10.0.26100.0"
} else {
    "$($buildTools.Name); Microsoft Windows SDK 10.0.26100.0"
}
Copy-NoticeFile $entries $windowsSdkComponent `
    (Join-Path $WindowsSdkLicenseDirectory "sdk_license.rtf") `
    "Microsoft-Windows-SDK-10.0.26100.0-license.rtf"
Copy-NoticeFile $entries $windowsSdkComponent `
    (Join-Path $WindowsSdkLicenseDirectory "sdk_third_party_notices.rtf") `
    "Microsoft-Windows-SDK-10.0.26100.0-third-party-notices.rtf"

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("RTX Video Converter - Third-Party Notices")
$lines.Add("=============================================")
$lines.Add("")
$lines.Add("This index covers third-party code and runtime files redistributed with the")
$lines.Add("application, in addition to the separately shipped FFmpeg LGPL license,")
$lines.Add("nv-codec-headers MIT notice, and NVIDIA RTX Video SDK license.")
$lines.Add("")
$lines.Add("The original, unmodified license and notice files are stored in the")
$lines.Add("THIRD_PARTY_LICENSES directory. SHA-256 values below bind this inventory to")
$lines.Add("the exact files included in this package.")
$lines.Add("")
foreach ($entry in $entries) {
    $lines.Add("Component: $($entry.Component)")
    $lines.Add("File: $($entry.File)")
    $lines.Add("SHA256: $($entry.SHA256)")
    $lines.Add("")
}

$indexPath = Join-Path $OutputDirectory "THIRD_PARTY_NOTICES.txt"
[System.IO.File]::WriteAllLines($indexPath, $lines, [System.Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    Index = $indexPath
    Components = ($entries.Component | Sort-Object -Unique).Count
    NoticeFiles = $entries.Count
    LicensesDirectory = $licensesDirectory
}
