$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'common.ps1')
try {
    Assert-Bundle $root
    if (-not (Test-Administrator)) {
        $p=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $PSCommandPath) -WindowStyle Hidden -Wait -PassThru
        exit $p.ExitCode
    }
    $script=Join-Path $PSScriptRoot 'clipboard\install.ps1'
    $p=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "{0}" -Restore' -f $script) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $PSScriptRoot 'restore-official-output.txt') -RedirectStandardError (Join-Path $PSScriptRoot 'restore-official-error.txt')
    Get-Content -LiteralPath (Join-Path $PSScriptRoot 'restore-official-output.txt')
    if ($p.ExitCode -ne 0) { throw 'Official binary restore failed; see restore-official-error.txt.' }
} catch { Write-Output ('FAIL: '+$_.Exception.Message);exit 1 }
