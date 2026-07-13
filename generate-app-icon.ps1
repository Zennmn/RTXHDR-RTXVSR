param(
    [string]$SourcePng = "",
    [string]$OutputPng = "",
    [string]$OutputIco = "",
    [string]$VersionedIco = "",
    [ValidateRange(16, 96)]
    [int]$CornerRadius = 48
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($SourcePng)) {
    $SourcePng = Join-Path $root "frontend\rtx-video-converter-winui\Assets\app-icon.png"
}
if ([string]::IsNullOrWhiteSpace($OutputPng)) {
    $OutputPng = $SourcePng
}
if ([string]::IsNullOrWhiteSpace($OutputIco)) {
    $OutputIco = Join-Path $root "frontend\rtx-video-converter-winui\Assets\app-icon.ico"
}
if ([string]::IsNullOrWhiteSpace($VersionedIco)) {
    $VersionedIco = Join-Path $root "frontend\rtx-video-converter-winui\Assets\app-icon-rounded-v100.ico"
}

$SourcePng = [System.IO.Path]::GetFullPath($SourcePng)
$OutputPng = [System.IO.Path]::GetFullPath($OutputPng)
$OutputIco = [System.IO.Path]::GetFullPath($OutputIco)
$VersionedIco = [System.IO.Path]::GetFullPath($VersionedIco)
if (-not (Test-Path -LiteralPath $SourcePng -PathType Leaf)) {
    throw "Source icon was not found: $SourcePng"
}

function New-RoundedRectanglePath([int]$Size, [int]$Radius) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = $Radius * 2
    $path.AddArc(0, 0, $diameter, $diameter, 180, 90)
    $path.AddArc($Size - $diameter, 0, $diameter, $diameter, 270, 90)
    $path.AddArc($Size - $diameter, $Size - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc(0, $Size - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

$source = [System.Drawing.Bitmap]::FromFile($SourcePng)
try {
    if ($source.Width -ne 256 -or $source.Height -ne 256) {
        throw "The application icon source must be 256x256 pixels."
    }

    $scale = 4
    $maskLarge = [System.Drawing.Bitmap]::new(256 * $scale, 256 * $scale)
    $maskSmall = [System.Drawing.Bitmap]::new(256, 256)
    $rounded = [System.Drawing.Bitmap]::new(
        256,
        256,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $path = New-RoundedRectanglePath $maskLarge.Width ($CornerRadius * $scale)
        $graphics = [System.Drawing.Graphics]::FromImage($maskLarge)
        try {
            $graphics.Clear([System.Drawing.Color]::Black)
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.FillPath([System.Drawing.Brushes]::White, $path)
        }
        finally {
            $graphics.Dispose()
            $path.Dispose()
        }

        $graphics = [System.Drawing.Graphics]::FromImage($maskSmall)
        try {
            $graphics.Clear([System.Drawing.Color]::Black)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($maskLarge, 0, 0, 256, 256)
        }
        finally {
            $graphics.Dispose()
        }

        for ($y = 0; $y -lt 256; $y++) {
            for ($x = 0; $x -lt 256; $x++) {
                $pixel = $source.GetPixel($x, $y)
                $maskAlpha = $maskSmall.GetPixel($x, $y).R
                $alpha = [Math]::Min($pixel.A, $maskAlpha)
                $rounded.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $alpha,
                    $pixel.R,
                    $pixel.G,
                    $pixel.B))
            }
        }

        $temporaryPng = $OutputPng + ".tmp.png"
        $rounded.Save($temporaryPng, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $maskLarge.Dispose()
        $maskSmall.Dispose()
        $rounded.Dispose()
    }
}
finally {
    $source.Dispose()
}

Move-Item -LiteralPath $temporaryPng -Destination $OutputPng -Force

$iconSource = [System.Drawing.Bitmap]::FromFile($OutputPng)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$frames = [System.Collections.Generic.List[byte[]]]::new()
try {
    foreach ($size in $sizes) {
        $frame = [System.Drawing.Bitmap]::new(
            $size,
            $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($frame)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.DrawImage($iconSource, 0, 0, $size, $size)
            }
            finally {
                $graphics.Dispose()
            }

            $stream = [System.IO.MemoryStream]::new()
            try {
                $frame.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                $frames.Add($stream.ToArray())
            }
            finally {
                $stream.Dispose()
            }
        }
        finally {
            $frame.Dispose()
        }
    }
}
finally {
    $iconSource.Dispose()
}

$temporaryIco = $OutputIco + ".tmp"
$stream = [System.IO.File]::Create($temporaryIco)
$writer = [System.IO.BinaryWriter]::new($stream)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$frames.Count)
    $offset = 6 + (16 * $frames.Count)
    for ($index = 0; $index -lt $frames.Count; $index++) {
        $size = $sizes[$index]
        $encodedSize = if ($size -eq 256) { 0 } else { $size }
        $writer.Write([byte]$encodedSize)
        $writer.Write([byte]$encodedSize)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$index].Length)
        $writer.Write([uint32]$offset)
        $offset += $frames[$index].Length
    }
    foreach ($frame in $frames) {
        $writer.Write($frame)
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}
Move-Item -LiteralPath $temporaryIco -Destination $OutputIco -Force
Copy-Item -LiteralPath $OutputIco -Destination $VersionedIco -Force

[pscustomobject]@{
    Png = $OutputPng
    Ico = $OutputIco
    VersionedIco = $VersionedIco
    CornerRadius = $CornerRadius
    IcoFrames = $frames.Count
}
