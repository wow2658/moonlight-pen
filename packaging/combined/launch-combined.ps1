param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$app = Join-Path $root 'app'
$record = Get-Content -LiteralPath (Join-Path $root 'BUILD-INFO.json') -Raw | ConvertFrom-Json
if ($record.product -ne 'Moonlight-Pen-fullscreen-hevc') { throw 'Unexpected package identity.' }
if (-not (Test-Path -LiteralPath (Join-Path $app 'portable.dat'))) { throw 'Portable profile marker is missing.' }
if (@(Get-Process -Name Moonlight -ErrorAction SilentlyContinue).Count) {
    throw 'Moonlight is already running. Finish that session and close it normally before opening this build. No process was stopped.'
}
if (@($record.runtimeFiles).Count -ne 2) { throw 'Unexpected runtime identity list.' }
foreach ($file in $record.runtimeFiles) {
    if ($file.name -notin @('Moonlight.exe', 'avcodec-63.dll') -or $file.sha256 -notmatch '^[a-f0-9]{64}$') {
        throw 'Unexpected runtime identity entry.'
    }
    $path = Join-Path $app $file.name
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) {
        throw ('Runtime checksum mismatch: ' + $file.name)
    }
}
if ($CheckOnly) { Write-Output 'PASS: fullscreen client and HEVC security DLL verified. No application was launched.'; exit 0 }
Start-Process -FilePath (Join-Path $app 'Moonlight.exe') -WorkingDirectory $app
