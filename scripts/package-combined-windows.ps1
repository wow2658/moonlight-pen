param(
    [Parameter(Mandatory=$true)][string]$TrialArchive,
    [Parameter(Mandatory=$true)][string]$NormalAppDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9._-]+$')][string]$Tag
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$output = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$archivePath = [IO.Path]::GetFullPath($TrialArchive)
$normalApp = [IO.Path]::GetFullPath($NormalAppDirectory)
$expectedArchive = '59c6e1c151ec527d36c06734aacf5aa6b6cec020352d07a5b2cee7b2d55b0f8f'
$expectedClient = '208878791be99bf34f653cf0ceebd836bc05f1cee42e1140958cbb35ab3fcca8'
$expectedCodec = '078b14fd1297e87a086ad7ed9abaa6f0af9a8bad945bbd240e6c8ec8742e8ab1'
if (Test-Path -LiteralPath $output) { throw 'Output directory already exists; nothing overwritten.' }
if ($output.Equals($sourceRoot,[StringComparison]::OrdinalIgnoreCase) -or $output.StartsWith($sourceRoot+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build output must be outside the source checkout.'
}
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $expectedArchive) { throw 'Trial input checksum mismatch.' }
if ((Get-FileHash -LiteralPath (Join-Path $normalApp 'Moonlight.exe') -Algorithm SHA256).Hash -ne $expectedClient) { throw 'Fullscreen client input checksum mismatch.' }
New-Item -ItemType Directory -Path $output | Out-Null
$package = Join-Path $output '01-Laptop-Moonlight-Pen'
New-Item -ItemType Directory -Path $package | Out-Null
function Write-Utf8([string]$Path, [string]$Text, [bool]$Bom=$false) {
    [IO.File]::WriteAllText($Path, (($Text -replace "`r?`n","`r`n").TrimEnd("`r","`n")+"`r`n"), [Text.UTF8Encoding]::new($Bom))
}
function Checked-Path([string]$Relative) {
    if ($Relative -match '(^/|^[A-Za-z]:|\\)' -or $Relative.Split('/') -contains '..') { throw 'Unsafe archive entry.' }
    $path = [IO.Path]::GetFullPath((Join-Path $package $Relative))
    if (-not $path.StartsWith($package+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Archive entry escapes package.' }
    return $path
}
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName.EndsWith('/')) { continue }
        if ($entry.FullName -notmatch '^Moonlight-Pen-Security-Trial01/(app/.*|licenses/.*)$') { continue }
        $relative = $Matches[1]
        if ($relative -match '(?i)(Moonlight Game Streaming Project|/cache/|/boxart/|/qmlcache/|\.(ini|log|pem|key|pfx|p12|crt|cer|pdb|dmp|lnk)$)') {
            throw 'Personal or debug data in input runtime.'
        }
        $target = Checked-Path $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
    }
    $ffmpegRoot = Join-Path $package 'support\ffmpeg'
    New-Item -ItemType Directory -Path (Join-Path $ffmpegRoot 'patches') -Force | Out-Null
    $sourceEntry = @($zip.Entries | Where-Object FullName -eq 'Moonlight-Pen-Security-Trial01/support/ffmpeg-source/ffmpeg-source.zip')
    if ($sourceEntry.Count -ne 1) { throw 'Pinned FFmpeg source archive missing.' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($sourceEntry[0],(Join-Path $ffmpegRoot 'ffmpeg-source.zip'),$false)
} finally { $zip.Dispose() }
$clientTarget = Checked-Path 'app/Moonlight.exe'
Copy-Item -LiteralPath (Join-Path $normalApp 'Moonlight.exe') -Destination $clientTarget -Force
if ((Get-FileHash -LiteralPath (Join-Path $package 'app\avcodec-63.dll') -Algorithm SHA256).Hash -ne $expectedCodec) { throw 'Security DLL checksum mismatch.' }
if (-not (Test-Path -LiteralPath (Join-Path $package 'app\portable.dat'))) { throw 'Portable marker missing.' }
foreach ($name in @('moonlight-v20-hevc-rext.patch','windows-hevc-bounds.patch')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot ('ffmpeg\patches\'+$name)) -Destination (Join-Path $ffmpegRoot ('patches\'+$name))
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'ffmpeg\source.json') -Destination (Join-Path $ffmpegRoot 'source.json')
Write-Utf8 (Join-Path $ffmpegRoot 'README.txt') ("FFmpeg source and patch order are recorded in source.json.`nBuild conditions: https://github.com/wow2658/moonlight-pen/blob/$Tag/ffmpeg/README.md`nThe supplied base archive is ffmpeg-source.zip.`n")
Copy-Item -LiteralPath (Join-Path $sourceRoot 'LICENSE') -Destination (Join-Path $package 'LICENSE')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'packaging\combined\START.cmd') -Destination (Join-Path $package '문라이트 실행.cmd')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'packaging\combined\README.txt') -Destination (Join-Path $package '00-먼저 읽기.txt')
foreach ($name in @('launch-combined.ps1','copy-portable-settings.ps1')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot ('packaging\combined\'+$name)) -Destination (Join-Path $package ('support\'+$name))
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'packaging\client\DELETE.cmd') -Destination (Join-Path $package '삭제.cmd')
$uninstaller = [IO.File]::ReadAllText((Join-Path $sourceRoot 'packaging\client\uninstall-client.ps1')).Replace('@MOONLIGHT_SHA256@',$expectedClient)
Write-Utf8 (Join-Path $package 'support\uninstall-client.ps1') $uninstaller $true
$record = [ordered]@{
    product='Moonlight-Pen-fullscreen-hevc'
    release_tag=$Tag
    architecture='x64'
    assembly_method='Existing compiled fullscreen client plus HEVC security DLL; no source recompilation.'
    client_source_revision='3d9ea0df7fd857047603861b444eb61b4bca61df'
    fullscreen_source_commit='70597ef9acd7fb31260b9910e3267a80d9997a49'
    security_sources_commit='7f40e946ad8080af5bf30406f1f4d1e33164cc08'
    runtimeFiles=@(@{name='Moonlight.exe';sha256=$expectedClient},@{name='avcodec-63.dll';sha256=$expectedCodec})
}
Write-Utf8 (Join-Path $package 'BUILD-INFO.json') ($record | ConvertTo-Json -Depth 6)
$sourceNote = "Moonlight Pen: unofficial Windows build`nCorresponding source: https://github.com/wow2658/moonlight-pen/tree/$Tag`nSource archive: https://github.com/wow2658/moonlight-pen/archive/refs/tags/$Tag.zip`nLicense: GNU GPL v3. Third-party notices: licenses/.`nFFmpeg base source and patches: support/ffmpeg/.`nBuild conditions: ffmpeg/README.md at the corresponding source tag.`nThis package assembles existing compiled artifacts; it is not a new source compilation.`n"
Write-Utf8 (Join-Path $package 'SOURCE.txt') $sourceNote
$files = @(Get-ChildItem -LiteralPath $package -Recurse -File)
$owned = @($files | ForEach-Object {$_.FullName.Substring($package.Length+1).Replace('\','/')})
Write-Utf8 (Join-Path $package 'UNINSTALL-FILES.json') ([ordered]@{format=1;product='Moonlight-Pen-portable';files=$owned} | ConvertTo-Json -Depth 4)
$files = @(Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName)
$checksums = @($files | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_.FullName.Substring($package.Length+1).Replace('\','/') })
Write-Utf8 (Join-Path $package 'FILES-SHA256.txt') ($checksums -join "`n")
$archiveOutput = Join-Path $output '01-Laptop-Moonlight-Pen.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($package,$archiveOutput,[IO.Compression.CompressionLevel]::Optimal,$true)
$archiveHash = (Get-FileHash -LiteralPath $archiveOutput -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Utf8 (Join-Path $output 'SHA256SUMS.txt') ($archiveHash+'  01-Laptop-Moonlight-Pen.zip')
[pscustomobject]@{PackageDirectory=$package;Archive=$archiveOutput;ArchiveSHA256=$archiveHash;FileCount=$files.Count;Tag=$Tag} | ConvertTo-Json
