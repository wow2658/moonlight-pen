param([Parameter(Mandatory=$true)][string]$NormalAppDirectory)
$ErrorActionPreference='Stop'
$normalApp=[IO.Path]::GetFullPath($NormalAppDirectory).TrimEnd('\')
$trialApp=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\app')).TrimEnd('\')
if($normalApp.Equals($trialApp,[StringComparison]::OrdinalIgnoreCase)) { throw 'Source and trial folder are the same.' }
if(@(Get-Process -Name Moonlight -ErrorAction SilentlyContinue).Count) { throw 'Close Moonlight normally before copying a consistent profile. No process was stopped.' }
if(-not (Test-Path -LiteralPath (Join-Path $normalApp 'Moonlight.exe')) -or -not (Test-Path -LiteralPath (Join-Path $normalApp 'portable.dat'))) { throw 'Choose the app folder of the normal portable Moonlight.' }
$source=Join-Path $normalApp 'Moonlight Game Streaming Project'
$target=Join-Path $trialApp 'Moonlight Game Streaming Project'
if(-not (Test-Path -LiteralPath $source -PathType Container)) { throw 'No portable profile was found. Nothing copied.' }
if(Test-Path -LiteralPath $target) { throw 'A trial profile already exists; refusing to overwrite it.' }
$items=@(Get-Item -LiteralPath $normalApp,$source)+@(Get-ChildItem -LiteralPath $source -Recurse -Force)
if(@($items | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count) { throw 'Profile contains a redirected path. Nothing copied.' }
Copy-Item -LiteralPath $source -Destination $target -Recurse
Write-Host 'Copied settings only into this trial. The normal profile was not edited.'
Write-Host 'This copy may contain private pairing keys. Keep it local and do not upload it.'

