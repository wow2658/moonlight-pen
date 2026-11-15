param([Parameter(Mandatory=$true)][string]$ProfileDirectory,[Parameter(Mandatory=$true)][string]$Backend)
$ErrorActionPreference='Stop'
$header=Join-Path $ProfileDirectory 'device-profile.local.h'
$text=[IO.File]::ReadAllText($header)
$pattern='(?m)^#define MOONLIGHT_BACKEND_SHA256 "[^"]*"\r?$'
if ([regex]::Matches($text,$pattern).Count -ne 1) { throw 'Exactly one local backend pin is required.' }
$sha=[Security.Cryptography.SHA256]::Create()
$stream=[IO.File]::OpenRead($Backend)
try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
finally { $stream.Dispose();$sha.Dispose() }
$text=[regex]::Replace($text,$pattern,('#define MOONLIGHT_BACKEND_SHA256 "'+$hash+'"'))
[IO.File]::WriteAllText($header,$text,(New-Object Text.UTF8Encoding($false)))
'Pinned the rebuilt backend in the private profile. No device was changed.'
