[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

Write-Host 'Preparing RTXHRD+RTXVSR Windows environment...'
pwsh "$PSScriptRoot\configure_native.ps1"

Write-Host 'Environment check complete.'

