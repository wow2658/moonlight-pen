$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    Assert-Bundle $root
    $install=Get-SunshineDirectory
    if (-not $install) { throw 'Run this backup on the current Sunshine HOST PC before formatting.' }
    $config=Join-Path $install 'config\sunshine.conf'
    if (-not (Test-Path -LiteralPath $config)) { throw 'Default sunshine.conf not found; inspect any custom config location before backing up.' }
    $policy=Read-JsonFile (Join-Path $root 'settings-policy.json')
    $read=Read-SunshineSettings ([IO.File]::ReadAllText($config)) $policy.host
    $settings=[pscustomobject]$read.values
    Assert-Settings $settings $policy.host
    $backup=[ordered]@{format=1;created=(Get-Date -Format o);sunshineVersion=(Get-Item -LiteralPath (Join-Path $install 'sunshine.exe')).VersionInfo.ProductVersion;settings=$settings;skippedKeys=$read.skipped;apps=$null;appsWarning='';credentialsIncluded=$false}
    $raw=[IO.File]::ReadAllText($config)
    $appsPath=Join-Path $install 'config\apps.json'
    if ($raw -match '(?m)^\s*file_apps\s*=') {
        $backup.appsWarning='Custom file_apps is configured. App definitions were not copied; export them separately after review.'
    } elseif (Test-Path -LiteralPath $appsPath) {
        $appsText=[IO.File]::ReadAllText($appsPath)
        if ($appsText -match '(?i)(BEGIN [A-Z ]*PRIVATE KEY|password|passwd|api[_-]?key|access[_-]?token|secret|authorization|bearer\s)') {
            $backup.appsWarning='App definitions may contain secrets and were excluded. Review them separately.'
        } else {
            $apps=$appsText | ConvertFrom-Json
            # Environment variables can contain credentials. Never export them.
            $backup.apps=@{env=@{};apps=@($apps.apps)}
            if ($apps.env -and @($apps.env.PSObject.Properties).Count) { $backup.appsWarning='App environment variables were excluded; recreate necessary nonsecret values manually.' }
        }
    }
    $json=$backup | ConvertTo-Json -Depth 30
    $stamped=Join-Path $PSScriptRoot ('settings-backup-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json')
    Write-Utf8 $stamped $json
    Write-Utf8 (Join-Path $PSScriptRoot 'settings-backup.json') $json
    Write-Output ('PASS: saved '+$read.values.Count+' Sunshine settings, without pairing/authentication/key files.')
    Write-Output ('Saved in: '+$PSScriptRoot+'\settings-backup.json')
    if ($read.skipped.Count) { Write-Output ('Excluded or multiline settings: '+($read.skipped -join ', ')) }
    if ($backup.appsWarning) { Write-Output $backup.appsWarning }
    Write-Output 'Copy settings-backup.json with this complete bundle to an external drive/cloud before formatting. This file can contain private app paths; do not publish it.'
} catch { Write-Output ('FAIL: '+$_.Exception.Message);exit 1 }
