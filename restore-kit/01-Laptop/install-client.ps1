param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    Assert-Bundle $root
    $policy=Read-JsonFile (Join-Path $root 'settings-policy.json')
    $preferences=Read-JsonFile (Join-Path $PSScriptRoot 'preferences.json')
    $snapshot=Join-Path $PSScriptRoot 'settings-backup.json'
    if (Test-Path -LiteralPath $snapshot) {
        $saved=Read-JsonFile $snapshot
        if ($saved.format -ne 1) { throw 'Unsupported client settings backup.' }
        $preferences=$saved.settings
    }
    Assert-Settings $preferences $policy.client
    if ($CheckOnly) { Write-Output 'PASS: laptop payload, settings allowlist and hashes. No files installed or devices toggled.';exit 0 }
    $target=Join-Path $env:LOCALAPPDATA 'Moonlight-Pen-20261115'
    $exe=Join-Path $target 'app\Moonlight.exe'
    if (Get-Process -Name Moonlight -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) { throw 'Close the restored Moonlight client before installing again.' }
    if (Test-Path -LiteralPath $target) {
        if ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected client directory is unsupported.' }
        $marker=Join-Path $target 'restore-version.txt'
        if (-not (Test-Path -LiteralPath $marker) -or [IO.File]::ReadAllText($marker) -ne '20261115-r3') { throw 'Unknown existing client directory; nothing overwritten.' }
        if ((Get-FileHash -LiteralPath $exe).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'client\app\Moonlight.exe')).Hash) { throw 'An updated client already exists. Inspect before replacing it.' }
    } else { New-Item -ItemType Directory -Path $target | Out-Null }
    Copy-Item -Path (Join-Path $PSScriptRoot 'client\*') -Destination $target -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'start-client.ps1') -Destination (Join-Path $target 'start-client.ps1') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'START-CLIENT.cmd') -Destination (Join-Path $target 'START-CLIENT.cmd') -Force
    $configFolder=Join-Path $target 'app\Moonlight Game Streaming Project'
    if (-not (Test-Path -LiteralPath $configFolder)) { New-Item -ItemType Directory -Path $configFolder | Out-Null }
    $config=Join-Path $configFolder 'Moonlight.ini'
    $text='';if (Test-Path -LiteralPath $config) { $text=[IO.File]::ReadAllText($config) }
    Write-Utf8 $config (Merge-IniSection $text 'General' (ConvertTo-SettingsTable $preferences))
    Write-Utf8 (Join-Path $target 'restore-version.txt') '20261115-r3'
    $shell=New-Object -ComObject WScript.Shell
    $linkPath=Join-Path ([Environment]::GetFolderPath('Desktop')) '문라이트 복원판.lnk'
    if (Test-Path -LiteralPath $linkPath) { if ($shell.CreateShortcut($linkPath).TargetPath -ne (Join-Path $target 'START-CLIENT.cmd')) { throw 'A different desktop shortcut already exists.' } }
    $link=$shell.CreateShortcut($linkPath);$link.TargetPath=Join-Path $target 'START-CLIENT.cmd';$link.WorkingDirectory=$target;$link.IconLocation=$exe;$link.Save()
    $adminScript=Join-Path $PSScriptRoot 'install-laptop-admin.ps1'
    $adminArgs='-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $adminScript
    $admin=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList $adminArgs -WindowStyle Hidden -PassThru -Wait
    if ($admin.ExitCode -ne 0) { throw 'Client restored, but finger-touch/firewall setup needs attention. See laptop-admin-result.txt.' }
    Write-Output ('PASS: client + pressure profiles + screen UI preferences + touch hotkey + program firewall rules. Installed at '+$target)
    Write-Output 'Next: pair with the host again. Enable text clipboard sharing in the host tile menu, or restart this client once after pairing.'
} catch { Write-Output ('FAIL: '+$_.Exception.Message);exit 1 }
