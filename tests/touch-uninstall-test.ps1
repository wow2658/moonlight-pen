param([Parameter(Mandatory=$true)][string]$SetupSource)
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText($SetupSource)
$base=Join-Path ([IO.Path]::GetTempPath()) ('touch-remove-fixture-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base | Out-Null
function Hash($Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
foreach($case in @('release','known-original','unknown')) {
    $root=Join-Path $base $case;$install=Join-Path $root 'installed';$payload=Join-Path $root 'app'
    New-Item -ItemType Directory -Path $install,$payload,(Join-Path $root 'links') | Out-Null
    foreach($name in @('TouchToggle.exe','TouchDispatcher.exe')) { [IO.File]::WriteAllText((Join-Path $payload $name),('release fixture '+$name)) }
    $backendPin=Hash (Join-Path $payload 'TouchToggle.exe');$receiverPin=Hash (Join-Path $payload 'TouchDispatcher.exe')
    foreach($name in @('TouchToggle.exe','TouchDispatcher.exe')) { [IO.File]::WriteAllText((Join-Path $install $name),$(if($case -eq 'release') {'release fixture '+$name} else {$case+' fixture '+$name})) }
    [IO.File]::WriteAllText((Join-Path $install 'dispatcher-events.log'),'fixture log')
    $oldBackend=Hash (Join-Path $install 'TouchToggle.exe');$oldReceiver=Hash (Join-Path $install 'TouchDispatcher.exe')
    $text=$source.Replace('@BACKEND_SHA256@',$backendPin).Replace('@DISPATCHER_SHA256@',$receiverPin)
    $text=$text.Replace('C:\Program Files\Laptop Finger Touch Toggle',$install)
    if($case -eq 'known-original') { $text=$text.Replace('DBE57D26D1A12D63FA023255D25D028CD91A98A016A953DE88970C4E4A274F74',$oldBackend).Replace('21B155262A7CF7EA516CC5F77DAA8D575DB914D48E79C0DCA3E5296E4B55179C',$oldReceiver) }
    $text=$text.Replace('if (-not $admin) {','if ($false) {')
    $programsReplacement="`$programs='"+(Join-Path $root 'links')+"'"
    $text=[regex]::Replace($text,'\$programs=Join-Path \(\[Environment\]::GetFolderPath\(''Programs''\)\) ''[^'']+''',[Text.RegularExpressions.MatchEvaluator]{ param($m) return $programsReplacement })
    $mock=@'
$script:fixtureRoot='@ROOT@'
$script:installed='@INSTALL@'
$script:events=Join-Path $script:fixtureRoot 'events.txt'
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$script:tasks=@{
 'MoonlightPen-TouchShortcutReceiver'=[pscustomobject]@{State='Running';Actions=@([pscustomobject]@{Execute=(Join-Path $script:installed 'TouchDispatcher.exe');Arguments='--listen'});Principal=[pscustomobject]@{UserId=$sid}};
 'MoonlightPen-LocalFingerTouchHotkey'=[pscustomobject]@{State='Disabled';Actions=@([pscustomobject]@{Execute=(Join-Path $script:installed 'TouchToggle.exe');Arguments='--listen'});Principal=[pscustomobject]@{UserId=$sid}}
}
function Verify-Protected { param($Path) if(-not $Path.StartsWith($script:fixtureRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Test path escaped fixture.' } }
function Run-Status { param($Exe) return 'touch=enabled; pen=healthy' }
function Get-ScheduledTask { param($TaskName,$ErrorAction) return $script:tasks[$TaskName] }
function Stop-ScheduledTask { param($TaskName) Add-Content -LiteralPath $script:events -Value ('stop '+$TaskName) }
function Unregister-ScheduledTask { param($TaskName,$Confirm,$ErrorAction) Add-Content -LiteralPath $script:events -Value ('remove '+$TaskName);$script:tasks.Remove($TaskName) }
function Get-Process { param($Name,$ErrorAction) return @() }
function Start-Process { param($FilePath,$ArgumentList,$WorkingDirectory,$WindowStyle,[switch]$Wait,[switch]$PassThru) if($FilePath -ne (Join-Path $script:installed 'TouchToggle.exe') -or ($ArgumentList -join ' ') -ne '--on --quiet') { throw 'Unexpected test process command.' };Add-Content -LiteralPath $script:events -Value 'touch-on';return [pscustomobject]@{ExitCode=0} }
function Get-Links { return @(@{Path=(Join-Path $script:fixtureRoot 'links\own.lnk');Args='--on'}) }
$shell=New-Object -ComObject WScript.Shell
$link=$shell.CreateShortcut((Join-Path $script:fixtureRoot 'links\own.lnk'));$link.TargetPath=Join-Path $script:installed 'TouchToggle.exe';$link.Arguments='--on';$link.Save()
'@
    $mock=$mock.Replace('@ROOT@',$root).Replace('@INSTALL@',$install)
    $marker="`ntry {`n"
    if(-not $text.Contains($marker)) { throw 'Outer setup entry not found.' }
    $text=$text.Replace($marker,("`n"+$mock+"`ntry {`n"))
    $script=Join-Path $root 'setup-under-test.ps1';[IO.File]::WriteAllText($script,$text,(New-Object Text.UTF8Encoding($true)))
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $script -Mode Uninstall
    $exit=$LASTEXITCODE
    if($case -eq 'unknown') {
        if($exit -ne 1 -or -not (Test-Path -LiteralPath $install) -or (Test-Path -LiteralPath (Join-Path $root 'events.txt'))) { throw 'Unknown install was modified.' }
    } else {
        if($exit -ne 0 -or (Test-Path -LiteralPath $install) -or (Test-Path -LiteralPath (Join-Path $root 'links'))) { throw ('Removal failed: '+$case) }
        $events=@(Get-Content -LiteralPath (Join-Path $root 'events.txt'))
        if($events[0] -ne 'touch-on' -or $events -notcontains 'remove MoonlightPen-TouchShortcutReceiver' -or $events -notcontains 'remove MoonlightPen-LocalFingerTouchHotkey') { throw 'Touch was not restored before removing both owned tasks.' }
    }
}
Write-Output 'PASS: release and initial known installation remove their files, log, shortcuts and both owned tasks after touch-on; unknown version unchanged. All device/task APIs mocked; only fixture data removed.'
