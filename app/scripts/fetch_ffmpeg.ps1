[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

Write-Host 'This repository expects an FFmpeg shared build with include/lib/bin.'
Write-Host 'Set FFMPEG_ROOT if your installation is not in the default path.'

