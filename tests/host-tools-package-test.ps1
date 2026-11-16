param([Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$FixtureRoot)
$ErrorActionPreference='Stop'
$fixture=Join-Path ([IO.Path]::GetFullPath($FixtureRoot)) ('host-package-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'support'),(Join-Path $fixture 'clipboard') | Out-Null
$target=Join-Path $fixture 'support\host-tools.ps1'
Copy-Item -LiteralPath $Source -Destination $target
$exe=Join-Path $fixture 'clipboard\sunshine.exe'
[IO.File]::WriteAllText($exe,'fixture bytes, not an executable')
$metadata=@{clipboardRevision='text-16mib-r4';maxTextBytes=16777216;patchedSha256=(Get-FileHash -LiteralPath $exe).Hash}
$metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'clipboard\package.json') -Encoding UTF8
foreach($relative in @('support\install-shortcuts.vbs','support\uninstall-shortcuts.vbs','clipboard\install.ps1','clipboard\validation.ps1')) {
    [IO.File]::WriteAllText((Join-Path $fixture $relative),'must never execute in CheckOnly')
}
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
& $ps -NoProfile -ExecutionPolicy Bypass -File $target -CheckOnly
if($LASTEXITCODE -ne 0) { throw 'Valid package check failed.' }
& $ps -NoProfile -ExecutionPolicy Bypass -File $target -CheckOnly -Remove
if($LASTEXITCODE -ne 0) { throw 'Valid removal package check failed.' }
[IO.File]::AppendAllText($exe,'tampered')
& $ps -NoProfile -ExecutionPolicy Bypass -File $target -CheckOnly 2>&1 | Out-Null
if($LASTEXITCODE -eq 0) { throw 'Tampered executable was accepted.' }
[IO.File]::WriteAllText($exe,'fixture bytes, not an executable')
$metadata.clipboardRevision='unknown'
$metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'clipboard\package.json') -Encoding UTF8
& $ps -NoProfile -ExecutionPolicy Bypass -File $target -CheckOnly 2>&1 | Out-Null
if($LASTEXITCODE -eq 0) { throw 'Unknown package revision was accepted.' }
'PASS: install/removal preflight, tamper rejection and mismatched revision; no services or real shortcuts touched.'
