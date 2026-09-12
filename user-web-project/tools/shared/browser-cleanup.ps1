param([Parameter(Mandatory=$true)][string]$Profile)
$ErrorActionPreference = 'Stop'
Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'chrome.exe' -or $_.Name -eq 'msedge.exe'
} | Where-Object {
    $_.CommandLine -and $_.CommandLine.Contains($Profile) -and $_.CommandLine -notmatch '--type='
} | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
