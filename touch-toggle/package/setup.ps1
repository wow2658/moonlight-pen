param([ValidateSet('Install','Check','Uninstall')][string]$Mode='Install',[switch]$ElevatedWindow)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
# Elevated setup must not auto-load a same-named module from a user's directory.
$env:PSModulePath=$PSHOME+'\Modules;'+$env:SystemRoot+'\System32\WindowsPowerShell\v1.0\Modules'
$folder='C:\Program Files\Laptop Finger Touch Toggle'
$taskName='MoonlightPen-TouchShortcutReceiver'
# Filled from the two release binaries by the packager, not a user profile.
$expected=@{'TouchToggle.exe'='@BACKEND_SHA256@';'TouchDispatcher.exe'='@DISPATCHER_SHA256@'}
$knownOriginal='DBE57D26D1A12D63FA023255D25D028CD91A98A016A953DE88970C4E4A274F74'
$knownReceiver='21B155262A7CF7EA516CC5F77DAA8D575DB914D48E79C0DCA3E5296E4B55179C'
$createdTask=$false
function File-Hash([string]$Path) {
    $algorithm=[Security.Cryptography.SHA256]::Create()
    $stream=[IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','') }
    finally { $stream.Dispose();$algorithm.Dispose() }
}
function No-Redirect([string]$Path) {
    $item=Get-Item -LiteralPath $Path -Force
    while ($item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'A redirected path needs manual review. No device was changed.' }
        $parent=Split-Path -Parent $item.FullName
        if (-not $parent -or $parent -eq $item.FullName) { break }
        $item=Get-Item -LiteralPath $parent -Force
    }
}
function Verify-Payload([string]$Base) {
    No-Redirect $Base
    foreach ($name in $expected.Keys) {
        $path=Join-Path $Base $name
        No-Redirect $path
        if ((File-Hash $path) -ne $expected[$name]) { throw ('File integrity failed: '+$name) }
    }
}
function Verify-Protected([string]$Path) {
    No-Redirect $Path
    $acl=Get-Acl -LiteralPath $Path
    $owner=$acl.GetOwner([Security.Principal.SecurityIdentifier]).Value
    if ($owner -notin @('S-1-5-18','S-1-5-32-544')) { throw 'Installation owner is not SYSTEM or Administrators.' }
    $write=[Security.AccessControl.FileSystemRights]'Write,Delete,DeleteSubdirectoriesAndFiles,ChangePermissions,TakeOwnership'
    foreach ($rule in $acl.GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier])) {
        if ($rule.AccessControlType -eq 'Allow' -and ($rule.FileSystemRights -band $write) -and $rule.IdentityReference.Value -notin @('S-1-5-18','S-1-5-32-544')) {
            throw 'Installation is writable by a non-administrator. Refusing elevated execution.'
        }
    }
}
function Run-Status([string]$Exe) {
    # DLL imports and later dynamic loads in release binaries use System32 only.
    # No shell, arbitrary command, network, clipboard or key text is accepted.
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$Exe;$start.Arguments='--status';$start.WorkingDirectory="$env:SystemRoot\System32"
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $process=[Diagnostics.Process]::Start($start)
    if (-not $process.WaitForExit(10000)) { throw 'Read-only device check timed out.' }
    $output=$process.StandardOutput.ReadToEnd();$errorText=$process.StandardError.ReadToEnd()
    if ($process.ExitCode -ne 0 -or $output -notmatch 'touch=(enabled|disabled); pen=healthy') { throw ('Device check failed: '+$output+$errorText) }
    return $output.Trim()
}
function Set-ProtectedDirectory {
    if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Path $folder | Out-Null }
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    foreach ($entry in @(@('S-1-5-18','FullControl'),@('S-1-5-32-544','FullControl'),@('S-1-5-32-545','ReadAndExecute'))) {
        $sid=New-Object Security.Principal.SecurityIdentifier($entry[0])
        $rule=New-Object Security.AccessControl.FileSystemAccessRule($sid,$entry[1],'ContainerInherit,ObjectInherit','None','Allow')
        $acl.AddAccessRule($rule)
    }
    $acl.SetOwner((New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544')))
    Set-Acl -LiteralPath $folder -AclObject $acl
    Verify-Protected $folder
}
function Get-Links {
    $desktop=[Environment]::GetFolderPath('Desktop')
    $programs=Join-Path ([Environment]::GetFolderPath('Programs')) '손가락 터치'
    return @(
        @{Path=(Join-Path $desktop '손터치 전환.lnk');Args='--toggle'},
        @{Path=(Join-Path $desktop '손터치 켜기.lnk');Args='--on'},
        @{Path=(Join-Path $programs '손터치 전환.lnk');Args='--toggle'},
        @{Path=(Join-Path $programs '손터치 켜기.lnk');Args='--on'},
        @{Path=(Join-Path $programs '손터치 끄기.lnk');Args='--off'}
    )
}
function Same-Principal([string]$Value,[string]$Sid) {
    try {
        if ($Value -match '^S-1-') { $resolved=New-Object Security.Principal.SecurityIdentifier($Value) }
        else { $account=New-Object Security.Principal.NTAccount($Value);$resolved=$account.Translate([Security.Principal.SecurityIdentifier]) }
        return $resolved.Value -eq $Sid
    } catch { return $false }
}
try {
    if ([Environment]::GetFolderPath('ProgramFiles') -ne 'C:\Program Files' -or -not [Environment]::Is64BitProcess) { throw 'Use Windows x64 with C:\Program Files.' }
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $admin=([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) {
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "{0}" -Mode {1} -ElevatedWindow' -f $PSCommandPath,$Mode
        $process=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait -PassThru
        exit $process.ExitCode
    }
    Verify-Payload (Join-Path $PSScriptRoot 'app')
    $status=Run-Status (Join-Path $PSScriptRoot 'app\TouchToggle.exe')
    if ($Mode -eq 'Check') { Write-Output ('PASS: release hashes + supported model + unique separate digitizers + '+$status+'. No installation or device transition.');exit 0 }
    $backend=Join-Path $folder 'TouchToggle.exe';$dispatcher=Join-Path $folder 'TouchDispatcher.exe'
    $task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if ($Mode -eq 'Uninstall') {
        if (-not (Test-Path -LiteralPath $folder)) { Write-Output 'Already absent.';exit 0 }
        Verify-Protected $folder;Verify-Payload $folder
        foreach ($path in @($backend,$dispatcher)) { Verify-Protected $path }
        if ($task) {
            if ($task.Actions.Count -ne 1 -or $task.Actions[0].Execute -ne $dispatcher -or $task.Actions[0].Arguments -ne '--listen' -or -not (Same-Principal $task.Principal.UserId $identity.User.Value)) { throw 'Different scheduled task; uninstall refused.' }
            Stop-ScheduledTask -TaskName $taskName
            Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
        }
        # The user explicitly invokes uninstall. Restore touch before removing
        # the program; never remove it while restoration reports failure.
        $process=Start-Process -FilePath $backend -ArgumentList '--on','--quiet' -WorkingDirectory $folder -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw 'Touch could not be enabled. Files were retained. Restart Windows before manual recovery.' }
        $shell=New-Object -ComObject WScript.Shell
        foreach ($entry in (Get-Links)) {
            if (Test-Path -LiteralPath $entry.Path) {
                $link=$shell.CreateShortcut($entry.Path)
                if ($link.TargetPath -eq $backend -and $link.Arguments -eq $entry.Args) { No-Redirect $entry.Path;Remove-Item -LiteralPath $entry.Path }
            }
        }
        # No recursive removal and no data from other installs is deleted.
        foreach ($name in @('TouchToggle.exe','TouchDispatcher.exe')) { Remove-Item -LiteralPath (Join-Path $folder $name) }
        Write-Output 'Removed this release and its own task/shortcuts. Logs remain in Program Files. Confirm physical finger touch; restart Windows if necessary.'
        exit 0
    }
    if (Test-Path -LiteralPath $folder) {
        Verify-Protected $folder
        $hasBackend=Test-Path -LiteralPath $backend
        $hasReceiver=Test-Path -LiteralPath $dispatcher
        if (-not $hasBackend -and -not $hasReceiver) {
            # Our uninstaller retains only the protected status log. Permit a
            # reinstall into that directory without deleting or executing it.
            foreach ($item in @(Get-ChildItem -LiteralPath $folder -Force)) {
                if ($item.PSIsContainer -or $item.Name -ne 'dispatcher-events.log') { throw 'Unexpected files remain in the installation directory.' }
                Verify-Protected $item.FullName
            }
        } else {
        foreach ($path in @($backend,$dispatcher)) { Verify-Protected $path }
        $installedBackend=File-Hash $backend;$installedReceiver=File-Hash $dispatcher
        $same=($installedBackend -eq $expected['TouchToggle.exe'] -and $installedReceiver -eq $expected['TouchDispatcher.exe'])
        $original=($installedBackend -eq $knownOriginal -and $installedReceiver -eq $knownReceiver)
        if ($same -or $original) {
            if (-not $task -or $task.Actions.Count -ne 1 -or $task.Actions[0].Execute -ne $dispatcher -or $task.Actions[0].Arguments -ne '--listen' -or -not (Same-Principal $task.Principal.UserId $identity.User.Value) -or $task.State -ne 'Running') { throw 'Known files exist, but the task needs inspection. Nothing was overwritten.' }
            Write-Output 'PASS: a verified working installation is already running. It was retained unchanged. No restart or device transition.'
            exit 0
        }
        throw 'A different installation already exists. It was not overwritten. Keep a working installation; inspect any upgrade separately.'
        }
    }
    if ($task) { throw 'A task with this name exists; it was not overwritten.' }
    $old=Get-ScheduledTask -TaskName 'MoonlightPen-LocalFingerTouchHotkey' -ErrorAction SilentlyContinue
    if ($old -and $old.State -ne 'Disabled') { throw 'The experimental old receiver must stay disabled.' }
    No-Redirect 'C:\Program Files'
    $links=Get-Links
    foreach ($entry in $links) { if (Test-Path -LiteralPath $entry.Path) { throw 'A shortcut already exists; it was not overwritten.' } }
    Set-ProtectedDirectory
    foreach ($name in $expected.Keys) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('app\'+$name)) -Destination (Join-Path $folder $name) }
    Verify-Payload $folder
    foreach ($path in @($backend,$dispatcher)) { Verify-Protected $path }
    # The installed receiver verifies Windows input registration without changing it.
    $process=Start-Process -FilePath $dispatcher -ArgumentList '--inspect' -WorkingDirectory $folder -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw 'Windows touch registration is inconsistent. Files retained; no task or device transition.' }
    $sid=$identity.User.Value
    $action=New-ScheduledTaskAction -Execute $dispatcher -Argument '--listen' -WorkingDirectory $folder
    $trigger=New-ScheduledTaskTrigger -AtLogOn -User $sid
    $principal=New-ScheduledTaskPrincipal -UserId $sid -LogonType Interactive -RunLevel Highest
    $settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Verified 940X3M finger touch only. Ctrl+Alt+F10; no network or arbitrary command input.' | Out-Null
    $createdTask=$true
    Start-ScheduledTask -TaskName $taskName
    Start-Sleep -Milliseconds 800
    if ((Get-ScheduledTask -TaskName $taskName).State -ne 'Running') { throw 'Receiver did not stay running. Check Ctrl+Alt+F10 conflicts.' }
    $shell=New-Object -ComObject WScript.Shell
    foreach ($entry in $links) {
        $directory=Split-Path -Parent $entry.Path
        if (-not (Test-Path -LiteralPath $directory)) { New-Item -ItemType Directory -Path $directory | Out-Null }
        $link=$shell.CreateShortcut($entry.Path);$link.TargetPath=$backend;$link.Arguments=$entry.Args
        $link.WorkingDirectory=$folder;$link.Hotkey='';$link.Description='Ctrl+Alt+F10: finger touch only; S Pen preserved.';$link.Save()
    }
    Write-Output 'PASS: installed. No automatic OFF/ON test. Press Ctrl+Alt+F10 once to verify finger OFF and pen OK, then once more to verify finger ON.'
} catch {
    if ($createdTask) { Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue;Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue }
    Write-Output ('FAIL: '+$_.Exception.Message)
    exit 1
} finally {
    if ($ElevatedWindow) { Read-Host 'Press Enter to close this setup window' | Out-Null }
}
