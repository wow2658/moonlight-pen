param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    Assert-Bundle $root
    $policy=Read-JsonFile (Join-Path $root 'settings-policy.json')
    $snapshot=Join-Path $PSScriptRoot 'settings-backup.json'
    $backup=$null
    if (Test-Path -LiteralPath $snapshot) {
        $backup=Read-JsonFile $snapshot
        if ($backup.format -ne 1 -or $backup.credentialsIncluded -ne $false) { throw 'Unsupported host settings backup.' }
        Assert-Settings $backup.settings $policy.host
        if ($backup.apps) {
            $appText=$backup.apps | ConvertTo-Json -Depth 30
            if ($appText -match '(?i)(BEGIN [A-Z ]*PRIVATE KEY|password|passwd|api[_-]?key|access[_-]?token|secret|authorization|bearer\s)') { throw 'Review possible secrets in the app backup before restoring.' }
            if (@($backup.apps.PSObject.Properties.Name | Where-Object { $_ -notin @('apps','env') }).Count -or @($backup.apps.env.PSObject.Properties).Count) { throw 'Unsupported app backup fields or environment variables.' }
        }
    }
    if ($CheckOnly) { Write-Output 'PASS: host payload hashes and any supplied settings backup. No MSI/service/config/display changes.';exit 0 }
    if (-not (Test-Administrator)) {
        $argsForAdmin='-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $PSCommandPath
        $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList $argsForAdmin -WindowStyle Hidden -Wait -PassThru
        $result=Join-Path $PSScriptRoot 'host-install-result.txt'
        if (Test-Path -LiteralPath $result) { Get-Content -LiteralPath $result }
        exit $child.ExitCode
    }
    $install=Get-SunshineDirectory
    if (-not $install) {
        $msi=Join-Path $PSScriptRoot 'official\Sunshine-Windows-AMD64-installer.msi'
        $msiArgs='/i "{0}" /qn /norestart' -f $msi
        $process=Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList $msiArgs -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -eq 3010) { throw 'Sunshine installed and Windows requires a restart. Restart and run RESTORE-HOST.cmd again.' }
        if ($process.ExitCode -ne 0) { throw ('Official Sunshine MSI failed: '+$process.ExitCode) }
        $install=Get-SunshineDirectory
        if (-not $install) { throw 'Sunshine service is not ready after installation.' }
    }
    # Exact hash validation and transactional executable rollback are implemented
    # in the already tested clipboard r3 installer. Never downgrade another build.
    $patch=Join-Path $PSScriptRoot 'clipboard\install.ps1'
    $process=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $patch) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $PSScriptRoot 'clipboard-install-output.txt') -RedirectStandardError (Join-Path $PSScriptRoot 'clipboard-install-error.txt')
    if ($process.ExitCode -ne 0) { throw 'Clipboard patch installation failed. See clipboard-install-output/error.txt; unsupported Sunshine versions are not overwritten.' }

    $configFolder=Join-Path $install 'config'
    if (-not (Test-Path -LiteralPath $configFolder)) { New-Item -ItemType Directory -Path $configFolder | Out-Null }
    if ((Get-Item -LiteralPath $configFolder).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected host config directory needs inspection.' }
    $config=Join-Path $configFolder 'sunshine.conf'
    $apps=Join-Path $configFolder 'apps.json'
    $values=@{native_pen_touch='enabled'}
    if ($backup) { $values=ConvertTo-SettingsTable $backup.settings;$values['native_pen_touch']='enabled' }
    $text='';if (Test-Path -LiteralPath $config) { $text=[IO.File]::ReadAllText($config) }
    $newConfig=Merge-SunshineSettings $text $values
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $configBefore=Join-Path $configFolder ('sunshine.before-format-restore-'+$stamp+'.conf')
    $appsBefore=Join-Path $configFolder ('apps.before-format-restore-'+$stamp+'.json')
    $hadConfig=Test-Path -LiteralPath $config
    $hadApps=Test-Path -LiteralPath $apps
    if ($hadConfig) { Copy-Item -LiteralPath $config -Destination $configBefore }
    if ($hadApps -and $backup -and $backup.apps) { Copy-Item -LiteralPath $apps -Destination $appsBefore }
    $wasRunning=(Get-Service SunshineService).Status -eq 'Running'
    try {
        Stop-Service SunshineService -ErrorAction Stop
        (Get-Service SunshineService).WaitForStatus('Stopped',[TimeSpan]::FromSeconds(20))
        Write-Utf8 $config $newConfig
        if ($backup -and $backup.apps) { Write-Utf8 $apps ($backup.apps | ConvertTo-Json -Depth 30) }
        if ($wasRunning) { Start-Service SunshineService;(Get-Service SunshineService).WaitForStatus('Running',[TimeSpan]::FromSeconds(20)) }
    } catch {
        if ($hadConfig) { Copy-Item -LiteralPath $configBefore -Destination $config -Force }
        elseif (Test-Path -LiteralPath $config) { Remove-Item -LiteralPath $config }
        if ($hadApps -and (Test-Path -LiteralPath $appsBefore)) { Copy-Item -LiteralPath $appsBefore -Destination $apps -Force }
        elseif (-not $hadApps -and $backup -and $backup.apps -and (Test-Path -LiteralPath $apps)) { Remove-Item -LiteralPath $apps }
        if ($wasRunning) { Start-Service SunshineService -ErrorAction SilentlyContinue }
        throw
    }
    $shortcutScript=Join-Path $PSScriptRoot 'display-shortcuts\install-shortcuts.vbs'
    & "$env:SystemRoot\System32\cscript.exe" //nologo $shortcutScript
    if ($LASTEXITCODE -ne 0) { throw 'Sunshine restored, but display shortcuts need attention.' }
    $message='PASS: Sunshine 2026.914.233613 + text clipboard r3 + native pen/touch + display shortcuts.'
    if ($backup) { $message+=' Supplied host settings restored; reselect display/audio devices if Windows assigned new IDs.' }
    else { $message+=' No settings-backup.json was supplied: previous custom host settings were NOT restored.' }
    $message+=' Open https://localhost:47990 to create the web login and pair again. No automatic display-mode switch was performed.'
    $message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'host-install-result.txt') -Encoding UTF8
    Write-Output $message
} catch { $message='FAIL: '+$_.Exception.Message;$message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'host-install-result.txt') -Encoding UTF8;Write-Output $message;exit 1 }
