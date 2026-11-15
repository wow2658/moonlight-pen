param([Parameter(Mandatory=$true)][string]$Root,[switch]$FirewallOnly)
$ErrorActionPreference='Stop'
$env:PSModulePath=$PSHOME+'\Modules;'+$env:SystemRoot+'\System32\WindowsPowerShell\v1.0\Modules'
function Resolve-RemoveRoot([string]$Path) {
    $resolved=[IO.Path]::GetFullPath($Path).TrimEnd('\')
    $blocked=@([IO.Path]::GetPathRoot($resolved).TrimEnd('\'),$env:SystemRoot,$env:ProgramFiles,$env:USERPROFILE,[Environment]::GetFolderPath('Desktop'),[Environment]::GetFolderPath('MyDocuments'))
    if ($resolved -in $blocked -or -not (Test-Path -LiteralPath (Join-Path $resolved 'app\Moonlight.exe')) -or (Test-Path -LiteralPath (Join-Path $resolved '.git'))) { throw 'This is not an extracted 01 Moonlight installation.' }
    $item=Get-Item -LiteralPath $resolved -Force
    while($item) {
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected installation path; nothing deleted.' }
        $parent=Split-Path -Parent $item.FullName
        if(-not $parent -or $parent -eq $item.FullName) { break }
        $item=Get-Item -LiteralPath $parent -Force
    }
    return $resolved
}
function Relative-Path([string]$Base,[string]$Value) {
    if(-not $Value -or $Value.Contains(':') -or $Value.StartsWith('\') -or $Value.StartsWith('/') -or ($Value -split '[\\/]') -contains '..' -or ($Value -split '[\\/]') -contains '.git') { throw 'Invalid uninstall manifest path.' }
    $path=[IO.Path]::GetFullPath((Join-Path $Base $Value))
    if(-not $path.StartsWith($Base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Deletion target escapes the installation.' }
    return $path
}
function Read-RemovePlan([string]$Base) {
    $manifest=[IO.File]::ReadAllText((Join-Path $Base 'UNINSTALL-FILES.json')) | ConvertFrom-Json
    if($manifest.product -ne 'Moonlight-Pen-portable' -or $manifest.format -ne 1) { throw 'Unknown uninstall manifest.' }
    $expected='@MOONLIGHT_SHA256@'
    $algorithm=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead((Join-Path $Base 'app\Moonlight.exe'))
    try { $hash=([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','') } finally { $stream.Dispose();$algorithm.Dispose() }
    if($hash -ne $expected) { throw 'This Moonlight executable was changed; removal needs inspection.' }
    $owned=@{};foreach($relative in $manifest.files) { $owned[(Relative-Path $Base $relative)]=$true }
    foreach($relative in @('UNINSTALL-FILES.json','FILES-SHA256.txt')) { $owned[(Join-Path $Base $relative)]=$true }
    # Inspect the entire tree before deleting anything; never follow a junction.
    $queue=New-Object 'Collections.Generic.Queue[string]';$queue.Enqueue($Base)
    $directories=New-Object 'Collections.Generic.List[string]'
    while($queue.Count) {
        $directory=$queue.Dequeue();$directories.Add($directory)
        foreach($item in @(Get-ChildItem -LiteralPath $directory -Force)) {
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'A link exists inside the installation; nothing deleted.' }
            if($item.PSIsContainer) { $queue.Enqueue($item.FullName);continue }
            $relative=$item.FullName.Substring($Base.Length+1)
            if($relative -match '^app[\\/](Moonlight Game Streaming Project|cache|boxart|qmlcache)[\\/]' -or $relative -match '^app[\\/]Moonlight[^\\/]*\.log$') { $owned[$item.FullName]=$true }
        }
    }
    return @{files=@($owned.Keys);directories=@($directories.ToArray())}
}
function Remove-ClientFiles([string]$Base,[object]$Plan) {
    # No recursive deletion. Unknown personal files keep their directory alive.
    foreach($path in $Plan.files) { if(Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path -Force } }
    foreach($path in @($Plan.directories | Sort-Object Length -Descending)) {
        if((Test-Path -LiteralPath $path -PathType Container) -and -not @(Get-ChildItem -LiteralPath $path -Force).Count) { Remove-Item -LiteralPath $path }
    }
}
function Remove-ProgramFirewall([string]$Exe) {
    foreach($filter in @(Get-NetFirewallApplicationFilter -Program $Exe -ErrorAction SilentlyContinue)) {
        foreach($rule in @($filter | Get-NetFirewallRule)) {
            $programs=@($rule | Get-NetFirewallApplicationFilter)
            if($programs.Count -eq 1 -and $programs[0].Program -eq $Exe) { $rule | Remove-NetFirewallRule }
        }
    }
}
try {
    $Root=Resolve-RemoveRoot $Root
    $plan=Read-RemovePlan $Root
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $admin=([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $exe=Join-Path $Root 'app\Moonlight.exe'
    if($FirewallOnly) {
        if(-not $admin) { throw 'Firewall cleanup requires administrator rights.' }
        Remove-ProgramFirewall $exe
        exit 0
    }
    foreach($process in @(Get-Process -Name Moonlight -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe })) {
        [void]$process.CloseMainWindow()
        if(-not $process.WaitForExit(10000)) { throw 'Close this Moonlight window and run 삭제.cmd again.' }
    }
    if($admin) { Remove-ProgramFirewall $exe }
    else {
        # Only rule cleanup is elevated. User-owned portable files are deleted
        # by the original process without granting them extra filesystem rights.
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "{0}" -Root "{1}" -FirewallOnly' -f $PSCommandPath,$Root
        $process=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait -PassThru
        if($process.ExitCode -ne 0) { throw 'Firewall cleanup did not complete. Installation files were retained.' }
    }
    $shell=New-Object -ComObject WScript.Shell
    foreach($directory in @([Environment]::GetFolderPath('Desktop'),[Environment]::GetFolderPath('Programs'))) {
        foreach($file in @(Get-ChildItem -LiteralPath $directory -Filter '*.lnk' -File -ErrorAction SilentlyContinue)) {
            if($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            $link=$shell.CreateShortcut($file.FullName)
            if($link.TargetPath -eq $exe -or $link.TargetPath -eq (Join-Path $Root '문라이트 실행.cmd')) { Remove-Item -LiteralPath $file.FullName }
        }
    }
    Set-Location -LiteralPath $env:TEMP
    Remove-ClientFiles $Root $plan
    Write-Output '삭제 완료: 이 문라이트와 설정, 연결된 바로가기와 방화벽 규칙을 제거했습니다.'
    if(Test-Path -LiteralPath $Root) { Write-Output '직접 추가한 다른 파일은 남겨 두었습니다.' }
} catch { Write-Output ('삭제 중단: '+$_.Exception.Message);exit 1 }
finally { if(-not $FirewallOnly) { Read-Host 'Enter를 누르면 닫습니다' | Out-Null } }
