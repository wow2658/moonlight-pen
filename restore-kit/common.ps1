$ErrorActionPreference = 'Stop'
function Read-JsonFile([string]$Path) { return ([IO.File]::ReadAllText($Path) | ConvertFrom-Json) }
function Write-Utf8([string]$Path, [string]$Text) { [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false))) }
function Test-Administrator { return ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }
function Assert-Bundle([string]$Root) {
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
    $manifest = Read-JsonFile (Join-Path $Root 'manifest.json')
    $known=@{}
    foreach ($file in $manifest.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $Root $file.path))
        if (-not $path.StartsWith($rootFull,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid manifest path.' }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) { throw ('Package file changed: '+$file.path) }
        $known[$path]=$true
    }
    foreach ($relative in @('01-Laptop\client','02-Host\clipboard','02-Host\official','02-Host\display-shortcuts','03-Touch\app')) {
        $payload=Join-Path $Root $relative
        if (Test-Path -LiteralPath $payload) {
            foreach ($item in (Get-ChildItem -LiteralPath $payload -Recurse -File)) {
                if (-not $known.ContainsKey($item.FullName)) { throw ('Unlisted payload file: '+$item.Name) }
            }
        }
    }
}
function Read-IniSection([string]$Text,[string]$Name) {
    $result = @{}; $active = $false
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\[([^\]]+)\]\s*$') { $active=$Matches[1] -eq $Name; continue }
        if ($active -and $line -match '^([^=;#]+)=(.*)$') { $result[$Matches[1].Trim()]=$Matches[2] }
    }
    return $result
}
function Merge-IniSection([string]$Text,[string]$Name,[System.Collections.IDictionary]$Values) {
    $lines = New-Object 'Collections.Generic.List[string]'
    $active=$false; $found=$false; $written=@{}
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\[([^\]]+)\]\s*$') {
            if ($active) { foreach ($key in $Values.Keys) { if (-not $written.ContainsKey($key)) { $lines.Add($key+'='+$Values[$key]);$written[$key]=$true } } }
            $active=$Matches[1] -eq $Name; if ($active) { $found=$true }
        }
        if ($active -and $line -match '^([^=;#]+)=(.*)$' -and $Values.Contains($Matches[1].Trim())) {
            $key=$Matches[1].Trim();if (-not $written.ContainsKey($key)) { $lines.Add($key+'='+$Values[$key]);$written[$key]=$true };continue
        }
        $lines.Add($line)
    }
    if (-not $found) { $lines.Add('['+$Name+']') }
    foreach ($key in $Values.Keys) { if (-not $written.ContainsKey($key)) { $lines.Add($key+'='+$Values[$key]) } }
    return ($lines -join "`r`n")
}
function Assert-Settings([object]$Settings,[string[]]$Allowed) {
    foreach ($property in $Settings.PSObject.Properties) {
        if ($Allowed -notcontains $property.Name) { throw ('Settings contain an unsupported key: '+$property.Name) }
        if ($property.Value -isnot [string] -or $property.Value.Length -gt 4096 -or $property.Value -match '[\r\n]') { throw ('Invalid settings value: '+$property.Name) }
    }
}
function Get-SunshineDirectory {
    $service=Get-CimInstance Win32_Service -Filter "Name='SunshineService'"
    if (-not $service) { return $null }
    if ($service.PathName -match '^\s*"([^"]+)"') { $exe=$Matches[1] }
    elseif ($service.PathName -match '^\s*(.+?\.exe)(?:\s|$)') { $exe=$Matches[1] }
    else { throw 'Cannot identify the Sunshine service path.' }
    $base=Split-Path -Parent ([IO.Path]::GetFullPath($exe))
    foreach ($path in @($base,(Split-Path -Parent $base))) { if (Test-Path -LiteralPath (Join-Path $path 'sunshine.exe')) { return $path } }
    throw 'Sunshine service exists but its executable was not found.'
}
function Read-SunshineSettings([string]$Text,[string[]]$Allowed) {
    $values=@{}; $skipped=New-Object 'Collections.Generic.List[string]'
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\s*([a-zA-Z0-9_]+)\s*=\s*(.*?)\s*$') {
            $key=$Matches[1];$value=($Matches[2] -split '#',2)[0].Trim()
            if ($Allowed -contains $key -and $value -notmatch '^[\[\{]\s*$') { $values[$key]=$value }
            else { $skipped.Add($key) }
        } elseif ($line.Trim() -and $line -notmatch '^\s*#') { $skipped.Add('(multiline/unknown)') }
    }
    return @{values=$values;skipped=@($skipped.ToArray())}
}
function ConvertTo-SettingsTable([object]$Value) { $table=@{};foreach ($property in $Value.PSObject.Properties) { $table[$property.Name]=[string]$property.Value };return $table }
function Merge-SunshineSettings([string]$Text,[System.Collections.IDictionary]$Values) {
    $written=@{};$lines=New-Object 'Collections.Generic.List[string]'
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\s*([a-zA-Z0-9_]+)\s*=' -and $Values.Contains($Matches[1])) {
            $key=$Matches[1]
            if ($line -match '=\s*[\[\{]\s*$') { throw 'An existing multiline host setting needs manual review before replacement.' }
            if (-not $written.ContainsKey($key)) { $lines.Add($key+' = '+$Values[$key]);$written[$key]=$true };continue
        }
        $lines.Add($line)
    }
    foreach ($key in $Values.Keys) { if (-not $written.ContainsKey($key)) { $lines.Add($key+' = '+$Values[$key]) } }
    return ($lines -join "`r`n")
}
