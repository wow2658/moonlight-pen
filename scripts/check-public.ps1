param([switch]$History)
$ErrorActionPreference = 'Stop'
$publicRoot = Split-Path $PSScriptRoot
$publicBlockedExtensions = @('.ini','.log','.pem','.key','.pfx','.p12','.crt','.cer','.exe','.dll','.zip','.7z','.lnk','.dmp','.pdb','.obj','.lib')
$publicFiles = Get-ChildItem -LiteralPath $publicRoot -File -Force -Recurse | Where-Object {
    $_.FullName -notmatch '[\\/]\.git[\\/]'
}
$publicBadFiles = @($publicFiles | Where-Object {
    $_.Extension.ToLowerInvariant() -in $publicBlockedExtensions -or
    $_.Name -in @('.env','auth.json','authorized_keys','portable.dat') -or
    $_.Name -match '^id_(rsa|ed25519)' -or
    $_.Name -in @('device-profile.local.h','device-profile.local.json') -or
    $_.Name -like 'settings-backup*.json' -or
    $_.FullName -match '[\\/](cache|boxart|qmlcache|\.ssh|\.development|Moonlight Game Streaming Project)[\\/]'
})
if ($publicBadFiles.Count) {
    $publicBadFiles | ForEach-Object { Write-Error ('Excluded file found: ' + $_.FullName.Substring($publicRoot.Length + 1)) }
    exit 1
}
$publicGitleaks = Get-Command gitleaks -ErrorAction SilentlyContinue
if (-not $publicGitleaks) { throw 'Install Gitleaks and put it on PATH before scanning.' }
& $publicGitleaks.Source dir $publicRoot --redact --no-banner --max-target-megabytes 100 --exit-code 1
if ($LASTEXITCODE -ne 0) { throw 'Gitleaks detected a finding or failed; do not publish until reviewed.' }
if ($History) {
    & $publicGitleaks.Source git $publicRoot --log-opts='--all' --redact --no-banner --exit-code 1
    if ($LASTEXITCODE -ne 0) { throw 'Gitleaks history scan failed; do not publish until reviewed.' }
}
'PASS: excluded file types and Gitleaks scan.'
