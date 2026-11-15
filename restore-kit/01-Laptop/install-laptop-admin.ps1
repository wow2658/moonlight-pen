$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    if (-not (Test-Administrator)) { throw 'Administrator rights required.' }
    Assert-Bundle $root
    $touchScript=Join-Path $root '03-Touch\install.ps1'
    $process=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $touchScript) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw 'Touch setup did not complete. See 03-Touch/install-result.txt. Client settings are already restored.' }
    $exe=Join-Path $env:LOCALAPPDATA 'Moonlight-Pen-20261115\app\Moonlight.exe'
    if ((Get-FileHash -LiteralPath $exe).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'client\app\Moonlight.exe')).Hash) { throw 'Installed client hash differs.' }
    foreach ($protocol in @('TCP','UDP')) {
        $name='MoonlightPen-Restore-20261115-'+$protocol
        $existing=Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue
        if ($existing) {
            $filter=$existing | Get-NetFirewallApplicationFilter
            if ($filter.Program -ne $exe) { throw 'A different firewall rule already uses this name.' }
        } else { New-NetFirewallRule -Name $name -DisplayName ('Moonlight Pen restore '+$protocol) -Direction Inbound -Action Allow -Program $exe -Protocol $protocol -Profile Private,Public | Out-Null }
    }
    'PASS: touch receiver and two program-specific firewall rules.' | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'laptop-admin-result.txt') -Encoding UTF8
} catch { ('FAIL: '+$_.Exception.Message) | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'laptop-admin-result.txt') -Encoding UTF8;exit 1 }
