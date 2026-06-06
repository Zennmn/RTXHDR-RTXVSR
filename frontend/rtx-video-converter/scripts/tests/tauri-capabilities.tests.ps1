$capabilitiesPath = Join-Path $PSScriptRoot "..\..\src-tauri\capabilities\default.json"
$capabilities = Get-Content -LiteralPath $capabilitiesPath -Raw | ConvertFrom-Json

Describe "Tauri sidecar permissions" {
  It "allows the auth token argument used by the backend sidecar" {
    $spawnPermission = $capabilities.permissions | Where-Object { $_.identifier -eq "shell:allow-spawn" } | Select-Object -First 1
    $sidecarRule = $spawnPermission.allow | Where-Object { $_.name -eq "binaries/vsr_backend" } | Select-Object -First 1
    $args = @($sidecarRule.args)

    ($args -contains "--auth-token") | Should Be $true
  }
}
