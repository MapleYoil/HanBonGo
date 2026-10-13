$ErrorActionPreference = 'Stop'
$installRoot = Join-Path $env:ProgramFiles 'HanBonGo'
$dataRoot = Join-Path $env:LOCALAPPDATA 'HanBonGo'
New-Item -ItemType Directory -Force -Path $dataRoot | Out-Null
try {
    $state = Get-Content -LiteralPath (Join-Path $installRoot 'active-install.json') -Raw | ConvertFrom-Json
    $tool = Join-Path $state.versionRoot 'x64\ime-tool.exe'
    $trayUpdater = Join-Path $state.versionRoot 'updater\HanBonGo.Updater.exe'
    if (Test-Path -LiteralPath $trayUpdater) { $exitRequest = Start-Process -FilePath $trayUpdater -ArgumentList '--exit' -WindowStyle Hidden -PassThru; $exitRequest.WaitForExit() }
    Remove-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name 'HanBonGoUpdater' -ErrorAction SilentlyContinue
    & $tool --deactivate
    & $tool --user-uninstall
    if ($LASTEXITCODE -ne 0) { throw 'Could not remove the user keyboard registration.' }
    $languages = Get-WinUserLanguageList
    foreach ($language in $languages) {
        foreach ($tip in @($language.InputMethodTips)) {
            if ($tip -match '(?i)\{7b3b1841-a842-4bc5-') { [void]$language.InputMethodTips.Remove($tip) }
        }
    }
    $defaultTip = (Get-WinDefaultInputMethodOverride).InputMethodTip
    if ($defaultTip -match '(?i)\{7b3b1841-a842-4bc5-') {
        $fallbackTip = $null
        foreach ($language in $languages) { if ($language.InputMethodTips.Count -gt 0) { $fallbackTip = $language.InputMethodTips[0]; break } }
        Set-WinDefaultInputMethodOverride -InputTip $fallbackTip
    }
    Set-WinUserLanguageList -LanguageList $languages -Force
    $principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        & (Join-Path $PSScriptRoot 'uninstall.ps1')
    } else {
        $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $PSScriptRoot 'uninstall.ps1') + '"'
        $process = Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -PassThru
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw 'System unregistration failed.' }
    }
    $startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) '한본어 입력기'
    foreach ($name in @('한본어 입력기 업데이터.lnk', '한본어 입력기 제거.lnk')) {
        $path = Join-Path $startMenu $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }
    if ((Test-Path -LiteralPath $startMenu) -and @(Get-ChildItem -LiteralPath $startMenu -Force).Count -eq 0) { Remove-Item -LiteralPath $startMenu }
    $startupLink = Join-Path ([Environment]::GetFolderPath('Startup')) '한본어 입력기 업데이터.lnk'
    if (Test-Path -LiteralPath $startupLink) { Remove-Item -LiteralPath $startupLink -Force }
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show('한본어 입력기를 제거했습니다. 사용 중인 DLL 파일은 남아 있을 수 있습니다.', '한본어 입력기 제거')
} catch {
    Add-Content -LiteralPath (Join-Path $dataRoot 'uninstall.log') -Value $_.Exception.Message -Encoding UTF8
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show(('제거하지 못했습니다. ' + $_.Exception.Message), '한본어 입력기 제거')
    exit 1
}
