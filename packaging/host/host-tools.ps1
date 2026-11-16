param([switch]$Remove,[switch]$CheckOnly,[switch]$Elevated)
$ErrorActionPreference='Stop'
$env:PSModulePath=$PSHOME+'\Modules;'+$env:SystemRoot+'\System32\WindowsPowerShell\v1.0\Modules'
$root=Split-Path $PSScriptRoot
$clipboard=Join-Path $root 'clipboard'
$manifest=Get-Content -LiteralPath (Join-Path $clipboard 'package.json') -Raw | ConvertFrom-Json
if($manifest.clipboardRevision -ne 'text-16mib-r4' -or $manifest.maxTextBytes -ne 16777216) { throw 'Expected the matching r4 clipboard package.' }
if((Get-FileHash -LiteralPath (Join-Path $clipboard 'sunshine.exe') -Algorithm SHA256).Hash -ne $manifest.patchedSha256) { throw 'Host executable hash mismatch. Download and extract the entire ZIP again.' }
$shortcuts=Join-Path $PSScriptRoot $(if($Remove){'uninstall-shortcuts.vbs'}else{'install-shortcuts.vbs'})
foreach($path in @((Join-Path $clipboard 'install.ps1'),(Join-Path $clipboard 'validation.ps1'),$shortcuts)) {
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'A required host package file is missing.' }
}
if($CheckOnly) { Write-Output 'PASS: matching r4 manifest, executable hash and install/removal files. No service or shortcuts were changed.'; exit 0 }
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(-not $admin) {
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Elevated'
    if($Remove) { $arguments+=' -Remove' }
    # This is the interactive installation console; keep its result visible.
    $child=Start-Process -FilePath $ps -ArgumentList $arguments -Verb RunAs -WindowStyle Normal -Wait -PassThru
    $child.Refresh();exit $child.ExitCode
}
$code=0
try {
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $clipboard 'install.ps1'))
    if($Remove) { $arguments+='-Restore' }
    # Run in a child process: the existing installer may exit after an idempotent check.
    & $ps @arguments
    if($LASTEXITCODE -ne 0) { throw 'Clipboard installation/restoration failed; screen shortcuts were left unchanged.' }
    & (Join-Path $env:SystemRoot 'System32\cscript.exe') //nologo $shortcuts
    if($LASTEXITCODE -ne 0) { throw 'Clipboard step completed, but screen shortcut registration/removal failed. Keep this package and report the error.' }
    if($Remove) { Write-Host 'PASS: original Sunshine restored and Moonlight screen shortcuts removed. Sunshine settings and pairing were preserved.' }
    else { Write-Host 'PASS: r4 clipboard and screen shortcuts installed. Reconnect the matching Moonlight client and test both directions.' }
} catch {
    $code=1;Write-Host ('FAILED: '+$_.Exception.Message) -ForegroundColor Red
}
[void](Read-Host 'Press Enter to close')
exit $code
