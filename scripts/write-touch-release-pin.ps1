param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$sha=[Security.Cryptography.SHA256]::Create()
$stream=[IO.File]::OpenRead((Join-Path $OutputDirectory 'TouchToggle.exe'))
try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
finally { $stream.Dispose();$sha.Dispose() }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'backend-pin.h'),('#pragma once'+"`n"+'#define MOONLIGHT_BACKEND_SHA256 "'+$hash+'"'+"`n"),(New-Object Text.UTF8Encoding($false)))
