param(
    [Parameter(Mandatory = $true)]
    [string]$ReferencePath,
    [Parameter(Mandatory = $true)]
    [string]$ImplementationPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [int]$X = 0,
    [int]$Y = 0,
    [int]$Width = 0,
    [int]$Height = 0
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$reference = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $ReferencePath))
$implementation = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $ImplementationPath))
try {
    if ($reference.Width -ne $implementation.Width -or $reference.Height -ne $implementation.Height) {
        throw "Reference and implementation must have the same dimensions."
    }

    if ($Width -le 0) { $Width = $reference.Width }
    if ($Height -le 0) { $Height = $reference.Height }
    if ($X -lt 0 -or $Y -lt 0 -or $X + $Width -gt $reference.Width -or $Y + $Height -gt $reference.Height) {
        throw "Crop rectangle is outside the image bounds."
    }

    $labelHeight = 32
    $canvas = [System.Drawing.Bitmap]::new(
        ($Width * 2),
        ($Height + $labelHeight),
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 245, 247, 250))
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

        $source = New-Object System.Drawing.Rectangle($X, $Y, $Width, $Height)
        $leftTarget = New-Object System.Drawing.Rectangle(0, $labelHeight, $Width, $Height)
        $rightTarget = New-Object System.Drawing.Rectangle($Width, $labelHeight, $Width, $Height)
        $graphics.DrawImage($reference, $leftTarget, $source, [System.Drawing.GraphicsUnit]::Pixel)
        $graphics.DrawImage($implementation, $rightTarget, $source, [System.Drawing.GraphicsUnit]::Pixel)

        $font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 30, 36, 46))
        try {
            $graphics.DrawString("REFERENCE", $font, $brush, 12, 8)
            $graphics.DrawString("IMPLEMENTATION", $font, $brush, $Width + 12, 8)
        }
        finally {
            $brush.Dispose()
            $font.Dispose()
        }
    }
    finally {
        $graphics.Dispose()
    }

    try {
        $directory = Split-Path -Parent $OutputPath
        if ($directory) {
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }
        $canvas.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $canvas.Dispose()
    }

    [pscustomobject]@{
        Path = (Resolve-Path -LiteralPath $OutputPath).Path
        Crop = "$X,$Y,$Width,$Height"
        Canvas = "$($Width * 2)x$($Height + $labelHeight)"
    }
}
finally {
    $implementation.Dispose()
    $reference.Dispose()
}
