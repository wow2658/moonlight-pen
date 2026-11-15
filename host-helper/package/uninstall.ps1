$ErrorActionPreference = 'Stop'
$helperDirectory = Join-Path $env:LOCALAPPDATA 'MoonlightDisplayHelper'
$helperInstalled = Join-Path $helperDirectory 'Moonlight-Display-Helper.exe'
Get-CimInstance Win32_Process -Filter "Name = 'Moonlight-Display-Helper.exe'" | Where-Object {
    $_.ExecutablePath -ieq $helperInstalled
} | ForEach-Object { Stop-Process -Id $_.ProcessId }
$helperStartup = Join-Path ([Environment]::GetFolderPath('Startup')) 'Moonlight Display Helper.lnk'
if (Test-Path -LiteralPath $helperStartup) { Remove-Item -LiteralPath $helperStartup }
if (Test-Path -LiteralPath $helperInstalled) { Remove-Item -LiteralPath $helperInstalled }
'자동 시작과 도우미 실행 파일을 제거했습니다. 화면 모드는 변경하지 않았고 helper.log는 남겨뒀습니다.'
