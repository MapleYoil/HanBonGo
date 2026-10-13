$ErrorActionPreference = 'Stop'
$installRoot = Join-Path $env:ProgramFiles 'HanBonGo'
$managementRoot = Join-Path $installRoot 'management'
$state = Get-Content -LiteralPath (Join-Path $installRoot 'active-install.json') -Raw | ConvertFrom-Json
$updater = Join-Path $state.versionRoot 'updater\HanBonGo.Updater.exe'
$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) '한본어 입력기'
$startup = [Environment]::GetFolderPath('Startup')
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
$shell = New-Object -ComObject WScript.Shell
$powerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
function Add-ManagementShortcut([string]$Path, [string]$Script, [string]$Extra = '') {
    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = $powerShell
    $shortcut.Arguments = '-NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + (Join-Path $managementRoot $Script) + '" ' + $Extra
    $shortcut.WorkingDirectory = $installRoot
    $shortcut.Description = '한본어 입력기'
    $shortcut.WindowStyle = 7
    $shortcut.Save()
}
$updaterShortcut = $shell.CreateShortcut((Join-Path $startMenu '한본어 입력기 업데이터.lnk'))
$updaterShortcut.TargetPath = $updater
$updaterShortcut.WorkingDirectory = $installRoot
$updaterShortcut.Description = '한본어 입력기 트레이 업데이터'
$updaterShortcut.Save()
Add-ManagementShortcut (Join-Path $startMenu '한본어 입력기 제거.lnk') 'uninstall-launcher.ps1'
$oldStartup = Join-Path $startup '한본어 입력기 업데이터.lnk'
if (Test-Path -LiteralPath $oldStartup) { Remove-Item -LiteralPath $oldStartup -Force }
$autoStart = $true
$settingsPath = Join-Path $env:LOCALAPPDATA 'HanBonGo\settings.json'
if (Test-Path -LiteralPath $settingsPath) {
    $settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
    if ($null -ne $settings.AutoStart) { $autoStart = [bool]$settings.AutoStart }
}
if ($autoStart) { Set-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name 'HanBonGoUpdater' -Value ('"' + $updater + '" --startup') }
else { Remove-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name 'HanBonGoUpdater' -ErrorAction SilentlyContinue }
Write-Host 'Native tray updater shortcut and startup preference registered.'
