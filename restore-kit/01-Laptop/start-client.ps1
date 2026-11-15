$ErrorActionPreference='Stop'
$app=Join-Path $PSScriptRoot 'app'
$config=Join-Path $app 'Moonlight Game Streaming Project\Moonlight.ini'
if (-not (Test-Path -LiteralPath (Join-Path $app 'portable.dat'))) { [IO.File]::WriteAllBytes((Join-Path $app 'portable.dat'),[byte[]]@()) }
# The user requested two-way text sharing. Enable it for saved hosts on the next
# launch; newly paired hosts can also enable it immediately in the tile menu.
if (Test-Path -LiteralPath $config) {
    $text=[IO.File]::ReadAllText($config)
    $indices=@([regex]::Matches($text,'(?m)^(\d+)\\uuid=') | ForEach-Object { $_.Groups[1].Value })
    foreach ($index in $indices) {
        $key=$index+'\shareClipboardText'
        $pattern='(?m)^'+[regex]::Escape($key)+'=[^\r\n]*'
        if ($text -match $pattern) { $text=[regex]::Replace($text,$pattern,$key+'=true') }
        else { $text=[regex]::Replace($text,'(?m)^\[hosts\]\r?$',('[hosts]'+"`r`n"+$key+'=true')) }
    }
    [IO.File]::WriteAllText($config,$text,(New-Object Text.UTF8Encoding($false)))
}
Start-Process -FilePath (Join-Path $app 'Moonlight.exe') -WorkingDirectory $app
