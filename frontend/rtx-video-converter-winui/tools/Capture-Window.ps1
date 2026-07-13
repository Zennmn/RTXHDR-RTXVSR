param(
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [string]$ProcessName = "RTXVideoConverter.WinUI",
    [int]$NormalizeWidth = 0,
    [int]$NormalizeHeight = 0,
    [int]$ForceFrameWidth = 0,
    [int]$ForceFrameHeight = 0
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if (-not ("PhysicalWindowCapture.NativeMethods" -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

namespace PhysicalWindowCapture
{
    public static class NativeMethods
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

        [DllImport("user32.dll")]
        public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);

        [DllImport("user32.dll")]
        public static extern bool IsIconic(IntPtr hwnd);

        [DllImport("user32.dll")]
        public static extern bool OpenIcon(IntPtr hwnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindowAsync(IntPtr hwnd, int command);

        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(
            IntPtr hwnd,
            IntPtr insertAfter,
            int x,
            int y,
            int width,
            int height,
            uint flags);

        [DllImport("dwmapi.dll")]
        public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attribute, out RECT value, int size);
    }
}
'@
}

$process = Get-Process -Name $ProcessName -ErrorAction Stop | Select-Object -First 1
$process.Refresh()
if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
    throw "Process '$ProcessName' does not have a main window."
}

$previousContext = [PhysicalWindowCapture.NativeMethods]::SetThreadDpiAwarenessContext([IntPtr](-4))
try {
    if ([PhysicalWindowCapture.NativeMethods]::IsIconic($process.MainWindowHandle)) {
        [void][PhysicalWindowCapture.NativeMethods]::OpenIcon($process.MainWindowHandle)
        [void][PhysicalWindowCapture.NativeMethods]::ShowWindowAsync($process.MainWindowHandle, 9)
        [void][PhysicalWindowCapture.NativeMethods]::SendMessage(
            $process.MainWindowHandle,
            0x0112,
            [IntPtr]0xF120,
            [IntPtr]::Zero)
        Start-Sleep -Milliseconds 350
    }

    $windowRect = New-Object PhysicalWindowCapture.NativeMethods+RECT
    $frameRect = New-Object PhysicalWindowCapture.NativeMethods+RECT
    [void][PhysicalWindowCapture.NativeMethods]::GetWindowRect($process.MainWindowHandle, [ref]$windowRect)
    $result = [PhysicalWindowCapture.NativeMethods]::DwmGetWindowAttribute(
        $process.MainWindowHandle,
        9,
        [ref]$frameRect,
        [Runtime.InteropServices.Marshal]::SizeOf([type][PhysicalWindowCapture.NativeMethods+RECT]))

    if ($result -ne 0) {
        $frameRect = $windowRect
    }

    if ($ForceFrameWidth -gt 0 -and $ForceFrameHeight -gt 0) {
        $invisibleWidth = ($windowRect.Right - $windowRect.Left) - ($frameRect.Right - $frameRect.Left)
        $invisibleHeight = ($windowRect.Bottom - $windowRect.Top) - ($frameRect.Bottom - $frameRect.Top)
        [void][PhysicalWindowCapture.NativeMethods]::SetWindowPos(
            $process.MainWindowHandle,
            [IntPtr]::Zero,
            $windowRect.Left,
            $windowRect.Top,
            $ForceFrameWidth + $invisibleWidth,
            $ForceFrameHeight + $invisibleHeight,
            0x0004)
        Start-Sleep -Milliseconds 350
        [void][PhysicalWindowCapture.NativeMethods]::GetWindowRect($process.MainWindowHandle, [ref]$windowRect)
        [void][PhysicalWindowCapture.NativeMethods]::DwmGetWindowAttribute(
            $process.MainWindowHandle,
            9,
            [ref]$frameRect,
            [Runtime.InteropServices.Marshal]::SizeOf([type][PhysicalWindowCapture.NativeMethods+RECT]))
    }

    $windowWidth = $windowRect.Right - $windowRect.Left
    $windowHeight = $windowRect.Bottom - $windowRect.Top
    $frameWidth = $frameRect.Right - $frameRect.Left
    $frameHeight = $frameRect.Bottom - $frameRect.Top

    $fullBitmap = New-Object System.Drawing.Bitmap(
        $windowWidth,
        $windowHeight,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($fullBitmap)
    $deviceContext = $graphics.GetHdc()
    try {
        if (-not [PhysicalWindowCapture.NativeMethods]::PrintWindow($process.MainWindowHandle, $deviceContext, 2)) {
            throw "PrintWindow failed."
        }
    }
    finally {
        $graphics.ReleaseHdc($deviceContext)
        $graphics.Dispose()
    }

    $cropX = [Math]::Max(0, $frameRect.Left - $windowRect.Left)
    $cropY = [Math]::Max(0, $frameRect.Top - $windowRect.Top)
    $crop = New-Object System.Drawing.Rectangle($cropX, $cropY, $frameWidth, $frameHeight)
    $visibleBitmap = $fullBitmap.Clone($crop, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $fullBitmap.Dispose()

    try {
        $directory = Split-Path -Parent $OutputPath
        if ($directory) {
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }

        if ($NormalizeWidth -gt 0 -and $NormalizeHeight -gt 0) {
            $normalized = New-Object System.Drawing.Bitmap(
                $NormalizeWidth,
                $NormalizeHeight,
                [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $normalizedGraphics = [System.Drawing.Graphics]::FromImage($normalized)
            try {
                $normalizedGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $normalizedGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $normalizedGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $normalizedGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $normalizedGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $normalizedGraphics.DrawImage($visibleBitmap, 0, 0, $NormalizeWidth, $NormalizeHeight)
            }
            finally {
                $normalizedGraphics.Dispose()
            }

            try {
                $normalized.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $normalized.Dispose()
            }
        }
        else {
            $visibleBitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
    }
    finally {
        $visibleBitmap.Dispose()
    }

    [pscustomobject]@{
        Path = (Resolve-Path -LiteralPath $OutputPath).Path
        Width = if ($NormalizeWidth -gt 0) { $NormalizeWidth } else { $frameWidth }
        Height = if ($NormalizeHeight -gt 0) { $NormalizeHeight } else { $frameHeight }
        NativeWidth = $frameWidth
        NativeHeight = $frameHeight
        DpiAware = $true
    }
}
finally {
    if ($previousContext -ne [IntPtr]::Zero) {
        [void][PhysicalWindowCapture.NativeMethods]::SetThreadDpiAwarenessContext($previousContext)
    }
}
