$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'input-preferences.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$installRoot = Join-Path $env:ProgramFiles 'HanBonGo'
$previousDefaultTip = [string](Get-WinDefaultInputMethodOverride).InputMethodTip
$languages = Get-WinUserLanguageList
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    & (Join-Path $PSScriptRoot 'install.ps1')
} else {
    $installerScript = Join-Path $PSScriptRoot 'install.ps1'
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $installerScript + '"'
    $installProcess = Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -PassThru
    $installProcess.WaitForExit()
    if ($installProcess.ExitCode -ne 0) { throw "Installer failed ($($installProcess.ExitCode)). See $projectRoot\install.log" }
}
$statePath = Join-Path $installRoot 'active-install.json'
$newVersion = (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json).versionRoot
$newTool = Join-Path $newVersion 'x64\ime-tool.exe'
$identity = @(& $newTool --version)
if ($LASTEXITCODE -ne 0 -or $identity.Count -lt 1) { throw 'Cannot read the installed keyboard identity.' }
$tipOutput = @(& $newTool --input-tip)
if ($LASTEXITCODE -ne 0 -or $tipOutput.Count -ne 1 -or $tipOutput[0] -notmatch '^0411:\{[A-Fa-f0-9-]{36}\}\{[A-Fa-f0-9-]{36}\}$') { throw 'Cannot read the installed language profile identity.' }
$inputTip = [string]$tipOutput[0]
Update-HanBonGoKeyboardList $languages $inputTip
try {
    & $newTool --user-install
    if ($LASTEXITCODE -ne 0) { throw 'User keyboard installation failed (InstallLayoutOrTip).' }
    # InstallLayoutOrTip can append an already listed profile. Restore the
    # captured positions after native registration has completed.
    Set-WinUserLanguageList -LanguageList $languages -Force
} finally {
    Restore-HanBonGoInputPreference $previousDefaultTip $inputTip
}
& $newTool --user-status
if ($LASTEXITCODE -ne 0) { throw 'User keyboard installation verification failed.' }
& $newTool --refresh-list
if ($LASTEXITCODE -ne 0) { throw 'Keyboard list refresh failed.' }
foreach ($architecture in @('x64', 'Win32')) {
    & (Join-Path $newVersion "$architecture\ime-tool.exe") --registration
    if ($LASTEXITCODE -ne 0) { throw "Keyboard registration verification failed: $architecture" }
    & (Join-Path $newVersion "$architecture\ime-tool.exe") --branding (Join-Path $newVersion "$architecture\HangulJpIME.dll")
    if ($LASTEXITCODE -ne 0) { throw "Keyboard taskbar icon verification failed: $architecture" }
}
Write-Host 'Updated and installed in the user keyboard list. Existing input-method preferences preserved.'
& (Join-Path $PSScriptRoot 'register-shortcuts.ps1')
$trayUpdater = Join-Path $newVersion 'updater\HanBonGo.Updater.exe'
$runningUpdaters = @(Get-Process -Name 'HanBonGo.Updater' -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq (Get-Process -Id $PID).SessionId })
$exitRequest = Start-Process -FilePath $trayUpdater -ArgumentList '--exit' -WindowStyle Hidden -PassThru
$exitRequest.WaitForExit()
foreach ($running in $runningUpdaters) { if (-not $running.WaitForExit(5000)) { throw 'Previous tray updater did not exit; no process was forced to stop.' } }
Start-Process -FilePath $trayUpdater -ArgumentList '--startup' -WindowStyle Hidden
