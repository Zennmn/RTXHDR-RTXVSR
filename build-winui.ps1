param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Publish,
    [string]$BackendExe = ""
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$backendBuild = Join-Path $root "build\backend"
$testProject = Join-Path $root "frontend\rtx-video-converter-winui-core-tests\RTXVideoConverter.Core.Tests.csproj"
$winuiProject = Join-Path $root "frontend\rtx-video-converter-winui\RTXVideoConverter.WinUI.csproj"

cmake -S (Join-Path $root "backend") -B $backendBuild -DVSR_ENABLE_TESTS=ON -DVSR_ENABLE_FFMPEG=OFF -DVSR_ENABLE_RTX_SDK=OFF
if ($LASTEXITCODE -ne 0) { throw "Backend configuration failed." }
cmake --build $backendBuild --target vsr_backend_tests vsr_backend --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Backend build failed." }
ctest --test-dir $backendBuild -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Backend tests failed." }

dotnet test $testProject -c $Configuration
if ($LASTEXITCODE -ne 0) { throw "WinUI core tests failed." }
dotnet build $winuiProject -c $Configuration -p:Platform=x64 --no-restore
if ($LASTEXITCODE -ne 0) { throw "WinUI build failed." }

if ($Publish) {
    dotnet publish $winuiProject -c Release -p:Platform=x64 -p:PublishProfile=win-x64
    if ($LASTEXITCODE -ne 0) { throw "WinUI publish failed." }
    $publishDirectory = Join-Path $root "frontend\rtx-video-converter-winui\bin\publish\win-x64"
    $winuiBuildDirectory = Join-Path $root "frontend\rtx-video-converter-winui\bin\x64\Release\net8.0-windows10.0.19041.0\win-x64"
    if (-not (Test-Path -LiteralPath (Join-Path $winuiBuildDirectory "RTXVideoConverter.WinUI.pri"))) {
        throw "WinUI compiled resources were not found: $winuiBuildDirectory"
    }
    # dotnet publish omits unpackaged WinUI XBF/PRI resources. Overlay the verified
    # self-contained build output so the portable executable can load its XAML.
    Copy-Item -Path (Join-Path $winuiBuildDirectory "*") -Destination $publishDirectory -Recurse -Force
    $qaDirectory = Join-Path $publishDirectory "qa"
    if (Test-Path -LiteralPath $qaDirectory) {
        Remove-Item -LiteralPath $qaDirectory -Recurse -Force
    }
    $runtimeDirectory = Join-Path $publishDirectory "runtime"
    New-Item -ItemType Directory -Force -Path $runtimeDirectory | Out-Null

    if ([string]::IsNullOrWhiteSpace($BackendExe)) {
        $BackendExe = Join-Path $backendBuild "Release\vsr_backend.exe"
    }
    if (-not (Test-Path -LiteralPath $BackendExe)) {
        throw "Backend executable was not found: $BackendExe"
    }
    Copy-Item -LiteralPath $BackendExe -Destination (Join-Path $runtimeDirectory "vsr_backend.exe") -Force

    $backendRuntimeDirectory = Split-Path -Parent $BackendExe
    Get-ChildItem -LiteralPath $backendRuntimeDirectory -Filter "*.dll" -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $runtimeDirectory $_.Name) -Force
    }
}
