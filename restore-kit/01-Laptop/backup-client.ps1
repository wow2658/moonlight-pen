param([string]$ConfigPath)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    Assert-Bundle $root
    if (-not $ConfigPath) { $ConfigPath=Join-Path $env:LOCALAPPDATA 'Moonlight-Pen-20261115\app\Moonlight Game Streaming Project\Moonlight.ini' }
    if (-not (Test-Path -LiteralPath $ConfigPath)) { throw 'Restored client config was not found. The bundle already includes the 2026-11-15 settings; use this command after using the restored client, or supply -ConfigPath.' }
    $policy=Read-JsonFile (Join-Path $root 'settings-policy.json')
    $current=Read-IniSection ([IO.File]::ReadAllText($ConfigPath)) 'General'
    $defaults=Read-JsonFile (Join-Path $PSScriptRoot 'preferences.json')
    $settings=ConvertTo-SettingsTable $defaults
    foreach ($key in $policy.client) { if ($current.ContainsKey($key)) { $settings[$key]=$current[$key] } }
    Assert-Settings ([pscustomobject]$settings) $policy.client
    $json=@{format=1;created=(Get-Date -Format o);settings=$settings;pairingIncluded=$false} | ConvertTo-Json -Depth 6
    Write-Utf8 (Join-Path $PSScriptRoot ('settings-backup-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json')) $json
    Write-Utf8 (Join-Path $PSScriptRoot 'settings-backup.json') $json
    Write-Output 'PASS: pressure/video/UI settings saved to 01-Laptop/settings-backup.json. No keys, certificates, host addresses or pairing data were copied.'
} catch { Write-Output ('FAIL: '+$_.Exception.Message);exit 1 }
