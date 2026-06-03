param(
  [Parameter(Mandatory = $true)][string]$BackendExe,
  [Parameter(Mandatory = $true)][string]$InputPath,
  [Parameter(Mandatory = $true)][string]$OutputPath,
  [int]$Port = 49321,
  [int]$StartupTimeoutSeconds = 15,
  [int]$JobTimeoutSeconds = 180
)

$process = Start-Process -FilePath $BackendExe -ArgumentList "--port", $Port -PassThru -WindowStyle Hidden
try {
  $health = $null
  $startupDeadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
  do {
    try {
      $health = Invoke-RestMethod "http://127.0.0.1:$Port/api/health"
    }
    catch {
      Start-Sleep -Milliseconds 500
    }
  } while ($null -eq $health -and (Get-Date) -lt $startupDeadline)

  if ($null -eq $health) { throw "Backend did not become healthy within $StartupTimeoutSeconds seconds." }
  if (-not $health.ready) { throw "Backend health check failed." }

  $body = @{
    inputPath = $InputPath
    outputPath = $OutputPath
    processing = @{
      vsr = @{ enabled = $true; quality = 3; scale = 2.0 }
      hdr = @{ enabled = $true; contrast = 100; saturation = 100; middleGray = 44; maxLuminance = 1000 }
    }
    output = @{ container = "mp4"; videoCodec = "hevc"; audioMode = "copy"; subtitleMode = "copy-compatible" }
  } | ConvertTo-Json -Depth 8

  $created = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$Port/api/jobs" -Body $body -ContentType "application/json"
  $jobDeadline = (Get-Date).AddSeconds($JobTimeoutSeconds)
  do {
    if ((Get-Date) -ge $jobDeadline) {
      throw "Job did not finish within $JobTimeoutSeconds seconds."
    }
    Start-Sleep -Milliseconds 500
    $job = Invoke-RestMethod "http://127.0.0.1:$Port/api/jobs/$($created.id)"
    Write-Host "$($job.state) $($job.stage) $($job.progress)"
  } while ($job.state -eq "queued" -or $job.state -eq "running" -or $job.state -eq "canceling")

  if ($job.state -ne "succeeded") {
    throw "Job ended as $($job.state): $($job.error.code) $($job.error.message) $($job.error.details)"
  }
  if (-not (Test-Path $OutputPath)) {
    throw "Output file was not created."
  }
}
finally {
  Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}
