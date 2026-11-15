$ErrorActionPreference = 'Stop'
# Run on the Sunshine HOST PC, not on the pen laptop.
$sunshineHost = @(Get-Process -Name sunshine -ErrorAction SilentlyContinue).Count -gt 0
if (-not $sunshineHost) {
    $sunshineHost = @(Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '*sunshine*' }).Count -gt 0
}
if (-not $sunshineHost) { throw 'Sunshine 호스트를 찾지 못했습니다. 이 설치 파일은 노트북이 아닌 호스트 PC에서 실행하세요.' }
$helperSource = Join-Path $PSScriptRoot 'Moonlight-Display-Helper.exe'
$helperDirectory = Join-Path $env:LOCALAPPDATA 'MoonlightDisplayHelper'
$helperInstalled = Join-Path $helperDirectory 'Moonlight-Display-Helper.exe'
$helperTest = Start-Process -FilePath $helperSource -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
if ($helperTest.ExitCode -ne 0) { throw "도우미 자체 검사 실패: $($helperTest.ExitCode)" }
New-Item -ItemType Directory -Path $helperDirectory -Force | Out-Null
Get-CimInstance Win32_Process -Filter "Name = 'Moonlight-Display-Helper.exe'" | Where-Object {
    $_.ExecutablePath -ieq $helperInstalled
} | ForEach-Object { Stop-Process -Id $_.ProcessId }
Copy-Item -LiteralPath $helperSource -Destination $helperInstalled -Force
if ((Get-FileHash -LiteralPath $helperSource).Hash -ne (Get-FileHash -LiteralPath $helperInstalled).Hash) { throw '설치 파일 확인 실패' }
$helperStartup = Join-Path ([Environment]::GetFolderPath('Startup')) 'Moonlight Display Helper.lnk'
$helperShell = New-Object -ComObject WScript.Shell
$helperShortcut = $helperShell.CreateShortcut($helperStartup)
$helperShortcut.TargetPath = $helperInstalled
$helperShortcut.WorkingDirectory = $helperDirectory
$helperShortcut.Description = 'Moonlight 호스트 복제/확장 도우미'
$helperShortcut.Save()
$helperProcess = Start-Process -FilePath $helperInstalled -WorkingDirectory $helperDirectory -WindowStyle Hidden -PassThru
Start-Sleep -Milliseconds 800
if ($helperProcess.HasExited) { throw "도우미가 종료되었습니다: $($helperProcess.ExitCode)" }
[pscustomobject]@{
    Computer = $env:COMPUTERNAME
    Installed = $helperInstalled
    ProcessId = $helperProcess.Id
    StartupShortcut = $helperStartup
    Log = (Join-Path $helperDirectory 'helper.log')
    NextStep = '노트북 문라이트 화면 패널에서 ? 버튼으로 연결 확인'
} | Format-List
