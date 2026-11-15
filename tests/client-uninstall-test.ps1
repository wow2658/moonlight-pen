param([Parameter(Mandatory=$true)][string]$Source)
$ErrorActionPreference='Stop'
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('moonlight-remove-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$sha=[Security.Cryptography.SHA256]::Create()
$bytes=[Text.Encoding]::UTF8.GetBytes('fixture binary; not executable')
$pin=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','');$sha.Dispose()
$text=[IO.File]::ReadAllText($Source).Replace('@MOONLIGHT_SHA256@',$pin)
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
if($errors.Count) { throw 'Uninstaller syntax error.' }
foreach($name in @('Resolve-RemoveRoot','Relative-Path','Read-RemovePlan','Remove-ClientFiles','Remove-ProgramFirewall')) {
    $fn=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst]},$false) | Where-Object Name -eq $name)
    if($fn.Count -ne 1) { throw 'Missing uninstall function.' }
    . ([scriptblock]::Create($fn[0].Extent.Text))
}
$script:checks=0
function Assert($Value,$Message) { if(-not $Value) { throw $Message };$script:checks++ }
function Reject([scriptblock]$Code,$Message) { $failed=$false;try { &$Code | Out-Null } catch { $failed=$true };Assert $failed $Message }
function Make-Fixture($Name) {
    $root=Join-Path $fixture $Name
    New-Item -ItemType Directory -Path (Join-Path $root 'app\Moonlight Game Streaming Project'),(Join-Path $root 'support') | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $root 'app\Moonlight.exe'),$bytes)
    [IO.File]::WriteAllText((Join-Path $root 'support\uninstall-client.ps1'),'fixture script')
    [IO.File]::WriteAllText((Join-Path $root 'app\Moonlight Game Streaming Project\Moonlight.ini'),'fixture private configuration')
    $manifest=@{format=1;product='Moonlight-Pen-portable';files=@('app/Moonlight.exe','support/uninstall-client.ps1')}
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'UNINSTALL-FILES.json') -Encoding UTF8
    return $root
}
$root=Make-Fixture 'keep-unknown'
[IO.File]::WriteAllText((Join-Path $root 'my-drawing.txt'),'keep me')
$outside=Join-Path $fixture 'outside.txt';[IO.File]::WriteAllText($outside,'outside sentinel')
Assert ((Resolve-RemoveRoot $root) -eq $root) 'Valid root rejected.'
$plan=Read-RemovePlan $root;Remove-ClientFiles $root $plan
Assert (-not (Test-Path -LiteralPath (Join-Path $root 'app\Moonlight.exe'))) 'Executable remains.'
Assert (-not (Test-Path -LiteralPath (Join-Path $root 'app\Moonlight Game Streaming Project'))) 'Private config remains.'
Assert ([IO.File]::ReadAllText((Join-Path $root 'my-drawing.txt')) -eq 'keep me') 'Unrelated file deleted.'
Assert ([IO.File]::ReadAllText($outside) -eq 'outside sentinel') 'Outside file deleted.'
$root=Make-Fixture 'clean';Remove-ClientFiles $root (Read-RemovePlan $root)
Assert (-not (Test-Path -LiteralPath $root)) 'Empty installation directory remains.'
$root=Make-Fixture 'bad-manifest'
Reject { Relative-Path $root '..\outside.txt' } 'Traversal accepted.'
Reject { Relative-Path $root 'C:\Windows\test' } 'Absolute path accepted.'
Reject { Relative-Path $root '.git/config' } 'Git metadata path accepted.'
New-Item -ItemType Directory -Path (Join-Path $root '.git') | Out-Null
Reject { Resolve-RemoveRoot $root } 'Git checkout accepted as install root.'
[IO.File]::WriteAllText((Join-Path $root 'app\Moonlight.exe'),'changed')
Reject { Read-RemovePlan $root } 'Changed executable accepted.'

# Mock only firewall APIs. The actual machine firewall is never touched.
$script:deleted=New-Object 'Collections.Generic.List[string]'
$target='C:\fixture\app\Moonlight.exe'
$script:rules=@([pscustomobject]@{Name='own';Programs=@($target)},[pscustomobject]@{Name='other';Programs=@('C:\other\Moonlight.exe')},[pscustomobject]@{Name='shared';Programs=@($target,'Any')})
function Get-NetFirewallApplicationFilter {
    param([string]$Program,[Parameter(ValueFromPipeline=$true)]$InputObject)
    process { if($Program) { return $script:rules };foreach($p in $InputObject.Programs) { [pscustomobject]@{Program=$p} } }
}
function Get-NetFirewallRule { param([Parameter(ValueFromPipeline=$true)]$InputObject) process { return $InputObject } }
function Remove-NetFirewallRule { param([Parameter(ValueFromPipeline=$true)]$InputObject) process { $script:deleted.Add($InputObject.Name) } }
Remove-ProgramFirewall $target
Assert ($script:deleted.Count -eq 1 -and $script:deleted[0] -eq 'own') 'Unrelated firewall rule removed.'
Write-Output ('PASS: '+$script:checks+' client removal, private config, unknown file preservation, path/root/hash and firewall ownership checks. Only disposable fixture files removed.')
