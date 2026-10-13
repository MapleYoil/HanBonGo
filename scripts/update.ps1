$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$installRoot = Join-Path $env:ProgramFiles 'HanBonGo'
$statePath = Join-Path $installRoot 'active-install.json'
if (-not (Test-Path -LiteralPath $statePath)) { $statePath = Join-Path $env:ProgramFiles 'HangulJpIME\active-install.json' }
if (Test-Path -LiteralPath $statePath) {
    $oldVersion = (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json).versionRoot
    $oldTool = Join-Path $oldVersion 'x64\ime-tool.exe'
    if (Test-Path -LiteralPath $oldTool) {
        & $oldTool --deactivate
        # Older releases lack this command. Registration replacement still proceeds.
    }
}
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
$inputTip = '0411:' + $identity[0] + '{E97A77BE-9C2D-4E9D-81AD-6A2F76D9AB25}'
$languages = Get-WinUserLanguageList
$japanese = $languages | Where-Object { $_.LanguageTag -eq 'ja' -or $_.LanguageTag -eq 'ja-JP' } | Select-Object -First 1
if (-not $japanese) { throw 'Japanese language support is required.' }
foreach ($tip in @($japanese.InputMethodTips)) {
    if ($tip -match '(?i)\{7b3b1841-a842-4bc5-') { [void]$japanese.InputMethodTips.Remove($tip) }
}
[void]$japanese.InputMethodTips.Add($inputTip)
Set-WinUserLanguageList -LanguageList $languages -Force
Set-WinDefaultInputMethodOverride -InputTip $inputTip
if ((Get-WinDefaultInputMethodOverride).InputMethodTip -ne $inputTip) { throw 'Windows did not save the updated keyboard preference.' }
& $newTool --user-install
if ($LASTEXITCODE -ne 0) { throw 'User keyboard installation failed (InstallLayoutOrTip).' }
& $newTool --activate
if ($LASTEXITCODE -ne 0) { throw 'Installed, but automatic keyboard activation failed.' }
& $newTool --user-status
if ($LASTEXITCODE -ne 0) { throw 'User keyboard installation verification failed.' }
foreach ($architecture in @('x64', 'Win32')) {
    & (Join-Path $newVersion "$architecture\ime-tool.exe") --branding (Join-Path $newVersion "$architecture\HangulJpIME.dll")
    if ($LASTEXITCODE -ne 0) { throw "Keyboard taskbar icon verification failed: $architecture" }
}
Write-Host 'Updated, installed in the user keyboard list, and activated the new profile.'
& (Join-Path $PSScriptRoot 'register-shortcuts.ps1')
$trayUpdater = Join-Path $newVersion 'updater\HanBonGo.Updater.exe'
$runningUpdaters = @(Get-Process -Name 'HanBonGo.Updater' -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq (Get-Process -Id $PID).SessionId })
$exitRequest = Start-Process -FilePath $trayUpdater -ArgumentList '--exit' -WindowStyle Hidden -PassThru
$exitRequest.WaitForExit()
foreach ($running in $runningUpdaters) { if (-not $running.WaitForExit(5000)) { throw 'Previous tray updater did not exit; no process was forced to stop.' } }
Start-Process -FilePath $trayUpdater -ArgumentList '--startup' -WindowStyle Hidden
