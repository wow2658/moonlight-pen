param([switch]$Restore)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'validation.ps1')

# Test-only patch installer: the existing Sunshine configuration and pairing keys stay in place.
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { throw 'Run INSTALL.cmd (or RESTORE.cmd) as administrator on the HOST PC.' }
$service = Get-CimInstance Win32_Service -Filter "Name='SunshineService'"
if (-not $service) { throw 'SunshineService was not found. This test installer requires the installed Windows Sunshine service.' }
if ($service.PathName -match '^\s*"([^"]+)"') { $serviceExe = $Matches[1] }
elseif ($service.PathName -match '^\s*(.+?\.exe)(?:\s|$)') { $serviceExe = $Matches[1] }
else { throw 'Cannot identify the Sunshine service executable.' }
$serviceDir = Split-Path -Parent ([IO.Path]::GetFullPath($serviceExe))
$candidates = @($serviceDir, (Split-Path -Parent $serviceDir))
$installDir = $candidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'sunshine.exe') } | Select-Object -First 1
if (-not $installDir) { throw 'sunshine.exe was not found beside the installed service.' }
$target = Join-Path $installDir 'sunshine.exe'
$backup = Join-Path $installDir 'sunshine.before-clipboard-text-test.exe'
$manifest = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'package.json') | ConvertFrom-Json
$currentHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
$patchHashes = @($manifest.patchedSha256) + @($manifest.previousPatchedSha256)

if ($Restore) {
    if (-not (Test-Path -LiteralPath $backup)) { throw 'No clipboard-test backup exists. Nothing was changed.' }
    $restoreHash = (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash
    if (-not (Test-OfficialSunshineOriginal $restoreHash $manifest)) { throw 'The backup differs from the verified original Sunshine version. Nothing was changed.' }
    if ($currentHash -eq $restoreHash) { Write-Host 'Already restored.'; exit 0 }
    if ($patchHashes -notcontains $currentHash) { throw 'Sunshine changed after this test was installed. Refusing to overwrite another update.' }
    $replacement = $backup
} else {
    $replacement = Join-Path $PSScriptRoot 'sunshine.exe'
    if ((Get-FileHash -LiteralPath $replacement -Algorithm SHA256).Hash -ne $manifest.patchedSha256) { throw 'Test executable hash mismatch.' }
    if ($currentHash -eq $manifest.patchedSha256) { Write-Host 'Clipboard text test is already installed.'; exit 0 }
    $isPreviousPatch = @($manifest.previousPatchedSha256) -contains $currentHash
    if ($isPreviousPatch) {
        if (-not (Test-Path -LiteralPath $backup)) { throw 'The previous test patch has no original backup. Nothing was changed.' }
        if (-not (Test-OfficialSunshineOriginal (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash $manifest)) { throw 'The previous test backup is not a verified official original. Nothing was changed.' }
    } elseif (-not (Test-OfficialSunshineOriginal $currentHash $manifest)) {
        $version = (Get-Item -LiteralPath $target).VersionInfo.ProductVersion
        Write-Host ('Detected Sunshine version: ' + $version)
        Write-Host ('Detected executable SHA256: ' + $currentHash)
        throw 'Expected the official Sunshine 2026.914.233613 AMD64 MSI or lite ZIP executable. This file differs from both verified releases. No files were changed.'
    }
    # Check executable loading before stopping the current host. AV or DLL errors leave it intact.
    $check = Start-Process -FilePath $replacement -ArgumentList '--help' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
    if (-not $check.WaitForExit(15000)) { $check.Kill(); throw 'The test executable load check timed out. Current Sunshine was not changed.' }
    $check.Refresh()
    if ($check.ExitCode -ne 0) { throw ('The test executable could not start. Exit code: ' + $check.ExitCode) }
    if ($isPreviousPatch) {
        Write-Host 'Updating the previous clipboard test; preserving its verified original backup.'
    } elseif (Test-Path -LiteralPath $backup) {
        if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $currentHash) { throw 'An unexpected backup already exists. Nothing was changed.' }
    } else { Copy-Item -LiteralPath $target -Destination $backup }
}

# Disconnect an active stream before installing. Stop only Sunshine's own service.
$wasRunning = (Get-Service -Name SunshineService).Status -eq 'Running'
$rollback = Join-Path $installDir 'sunshine.clipboard-transaction.exe'
if (Test-Path -LiteralPath $rollback) { throw 'A previous interrupted installation needs inspection: sunshine.clipboard-transaction.exe' }
Copy-Item -LiteralPath $target -Destination $rollback
try {
    Stop-Service -Name SunshineService -ErrorAction Stop
    (Get-Service -Name SunshineService).WaitForStatus('Stopped', [TimeSpan]::FromSeconds(20))
    Copy-Item -LiteralPath $replacement -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $replacement -Algorithm SHA256).Hash) { throw 'Installed executable verification failed.' }
    if ($wasRunning) {
        Start-Service -Name SunshineService
        (Get-Service -Name SunshineService).WaitForStatus('Running', [TimeSpan]::FromSeconds(20))
        $started = $false
        for ($attempt = 0; $attempt -lt 10; $attempt++) {
            Start-Sleep -Milliseconds 500
            $child = Get-Process -Name sunshine -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target }
            if ($child) { $started = $true; break }
        }
        if (-not $started) { throw 'Sunshine service started but its streaming process did not appear.' }
    }
    Remove-Item -LiteralPath $rollback
    if ($Restore) { Write-Host 'RESTORED: original Sunshine executable. Configuration and pairing preserved.' }
    else { Write-Host 'INSTALLED: clipboard text test. Now reconnect from the test Moonlight client.' }
    Write-Host 'This checks installation only; verify two-way copy/paste from both PCs.'
} catch {
    $failure = $_
    try {
        Stop-Service -Name SunshineService -ErrorAction SilentlyContinue
        Copy-Item -LiteralPath $rollback -Destination $target -Force
        if ($wasRunning) { Start-Service -Name SunshineService }
        Remove-Item -LiteralPath $rollback
        Write-Host 'Original executable restored after the failed installation.'
    } catch { Write-Warning 'Automatic rollback failed. Keep sunshine.clipboard-transaction.exe for manual recovery.' }
    throw $failure
}
