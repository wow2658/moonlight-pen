param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$folder = 'C:\Program Files\Laptop Finger Touch Toggle'
$taskName = 'MoonlightPen-TouchShortcutReceiver'
$profilePath = Join-Path $PSScriptRoot 'device-profile.local.json'
if (-not (Test-Path -LiteralPath $profilePath)) { throw 'Private verified touch profile is missing. Assemble a private restore kit first.' }
$profile = [IO.File]::ReadAllText($profilePath) | ConvertFrom-Json
$expected = @{'TouchToggle.exe'=$profile.backendSha256; 'TouchDispatcher.exe'=$profile.dispatcherSha256}
foreach ($hash in $expected.Values) { if ($hash -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid local executable pin.' } }

function Assert-Device($id, $parent, $usage) {
    $device = Get-PnpDevice -InstanceId $id -PresentOnly -ErrorAction Stop
    $parentValue = (Get-PnpDeviceProperty -InstanceId $id -KeyName DEVPKEY_Device_Parent).Data
    $hardware = (Get-PnpDeviceProperty -InstanceId $id -KeyName DEVPKEY_Device_HardwareIds).Data
    $problem = (Get-PnpDeviceProperty -InstanceId $id -KeyName DEVPKEY_Device_ProblemCode).Data
    if ($parentValue -ne $parent -or $hardware -notcontains $usage -or $problem -ne 0) {
        throw 'Expected input device is not ready. Install the laptop drivers and restart; if IDs changed, this package needs to be adapted. No device was toggled.'
    }
    if ($device.Status -ne 'OK') { throw 'Input device is not healthy. No device was toggled.' }
}

function Assert-Files($base) {
    foreach ($name in $expected.Keys) {
        $path = Join-Path $base $name
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected[$name]) {
            throw ('File verification failed: ' + $name)
        }
    }
}

try {
    Assert-Files (Join-Path $PSScriptRoot 'app')
    Assert-Device $profile.touchId $profile.touchParent 'HID_DEVICE_UP:000D_U:0004'
    Assert-Device $profile.penId $profile.penParent 'HID_DEVICE_UP:000D_U:0002'
    if ($CheckOnly) {
        Write-Output 'PASS: package hashes and current touch/pen identities match. Read-only check; no installation or device changes.'
        exit 0
    }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $admin = ([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) {
        $argsForAdmin = '-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $PSCommandPath
        $child = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList $argsForAdmin -WindowStyle Hidden -Wait -PassThru
        $resultFile = Join-Path $PSScriptRoot 'install-result.txt'
        if (Test-Path -LiteralPath $resultFile) { Get-Content -LiteralPath $resultFile }
        exit $child.ExitCode
    }
    if ([Environment]::GetFolderPath('ProgramFiles') -ne 'C:\Program Files') { throw 'This tested build requires C:\Program Files.' }
    $backend = Join-Path $folder 'TouchToggle.exe'
    $dispatcher = Join-Path $folder 'TouchDispatcher.exe'
    $existingTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $folder) {
        if ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected installation directory is not supported.' }
        Assert-Files $folder
        if ($existingTask -and $existingTask.Actions.Execute -eq $dispatcher -and $existingTask.Actions.Arguments -eq '--listen' -and $existingTask.Principal.UserId -eq $identity.User.Value -and $existingTask.State -eq 'Running') {
            $message = 'PASS: the identical version is already installed and running. No changes were made.'
            $message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'install-result.txt') -Encoding UTF8
            Write-Output $message
            exit 0
        }
        throw 'An existing installation needs inspection. It was not overwritten. Use this package on a fresh Windows installation.'
    }
    if ($existingTask) { throw 'A task with this name already exists. It was not overwritten.' }
    $older = Get-ScheduledTask -TaskName 'MoonlightPen-LocalFingerTouchHotkey' -ErrorAction SilentlyContinue
    if ($older -and $older.State -ne 'Disabled') { throw 'The previous experimental receiver must remain disabled.' }

    $desktop = [Environment]::GetFolderPath('Desktop')
    $programs = Join-Path ([Environment]::GetFolderPath('Programs')) '손가락 터치'
    $links = @(
        @{Path=(Join-Path $desktop '손터치 전환.lnk'); Args='--toggle'},
        @{Path=(Join-Path $desktop '손터치 켜기.lnk'); Args='--on'},
        @{Path=(Join-Path $programs '손터치 전환.lnk'); Args='--toggle'},
        @{Path=(Join-Path $programs '손터치 켜기.lnk'); Args='--on'},
        @{Path=(Join-Path $programs '손터치 끄기.lnk'); Args='--off'}
    )
    foreach ($entry in $links) { if (Test-Path -LiteralPath $entry.Path) { throw ('Shortcut already exists; it was not overwritten: ' + $entry.Path) } }

    New-Item -ItemType Directory -Path $folder | Out-Null
    $acl = New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true, $false)
    foreach ($rule in @(@('S-1-5-18','FullControl'),@('S-1-5-32-544','FullControl'),@('S-1-5-32-545','ReadAndExecute'))) {
        $sid = New-Object Security.Principal.SecurityIdentifier($rule[0])
        $entry = New-Object Security.AccessControl.FileSystemAccessRule($sid,$rule[1],'ContainerInherit,ObjectInherit','None','Allow')
        $acl.AddAccessRule($entry)
    }
    $acl.SetOwner((New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544')))
    Set-Acl -LiteralPath $folder -AclObject $acl
    foreach ($name in $expected.Keys) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('app\'+$name)) -Destination (Join-Path $folder $name) }
    Assert-Files $folder

    # Read-only: verifies native Windows input registration and original backend.
    foreach ($mode in @('--inspect','--transport-test')) {
        $outputPath = Join-Path $folder ($mode.TrimStart('-')+'.txt')
        $process = Start-Process -FilePath $dispatcher -ArgumentList $mode -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $outputPath -RedirectStandardError ($outputPath+'.error')
        if ($process.ExitCode -ne 0) { throw ('Read-only input preflight failed: '+$mode) }
    }
    $sid = $identity.User.Value
    $action = New-ScheduledTaskAction -Execute $dispatcher -Argument '--listen' -WorkingDirectory $folder
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $sid
    $principal = New-ScheduledTaskPrincipal -UserId $sid -LogonType Interactive -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Ctrl+Alt+F10: fixed original finger-touch helper; independent Windows input checks.' | Out-Null
    $createdTask = $true
    Start-ScheduledTask -TaskName $taskName
    Start-Sleep -Milliseconds 800
    if ((Get-ScheduledTask -TaskName $taskName).State -ne 'Running') { throw 'Receiver did not remain running. Ctrl+Alt+F10 may be used by another app.' }
    if (-not (Test-Path -LiteralPath $programs)) { New-Item -ItemType Directory -Path $programs | Out-Null }
    $shell = New-Object -ComObject WScript.Shell
    foreach ($entry in $links) {
        $link = $shell.CreateShortcut($entry.Path)
        $link.TargetPath = $backend
        $link.Arguments = $entry.Args
        $link.WorkingDirectory = $folder
        $link.Hotkey = ''
        $link.Description = '손가락 터치 전환. S펜 유지. 단축키 Ctrl+Alt+F10.'
        $link.Save()
    }
    $message = 'PASS: installed and receiver running. No devices were toggled. Physically test OFF + S Pen, then ON, using Ctrl+Alt+F10 once per step.'
    $message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'install-result.txt') -Encoding UTF8
    Write-Output $message
} catch {
    if ($createdTask) {
        Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    }
    $message = 'FAIL: '+$_.Exception.Message+' No automatic touch OFF/ON test was performed.'
    $message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'install-result.txt') -Encoding UTF8
    Write-Output $message
    exit 1
}
