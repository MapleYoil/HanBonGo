$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'uninstall-common.ps1')
$installRoot = [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'HanBonGo'))
$dataRoot = Join-Path $env:LOCALAPPDATA 'HanBonGo'
$workRoot = Join-Path $dataRoot ('uninstall-work-' + [guid]::NewGuid().ToString('N'))
$logPath = Join-Path $dataRoot 'uninstall.log'
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null
try {
    $previousTip = [string](Invoke-HanBonGoUninstallStep 'Read default keyboard' { (Get-WinDefaultInputMethodOverride).InputMethodTip })
    $tool = ''
    $statePath = Join-Path $installRoot 'active-install.json'
    if (Test-Path -LiteralPath $statePath) {
        $state = Invoke-HanBonGoUninstallStep 'Read installation information' { Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json }
        $versionRoot = Assert-HanBonGoVersionPath $installRoot $state.versionRoot
        $sourceTool = Join-Path $versionRoot 'x64\ime-tool.exe'
        if (Test-Path -LiteralPath $sourceTool) {
            # Keep only the tool needed for original-user cleanup after the
            # administrator phase removes files. It does not load the IME DLL.
            $tool = Join-Path $workRoot 'ime-tool.exe'
            Invoke-HanBonGoUninstallStep 'Prepare user keyboard removal' { Copy-Item -LiteralPath $sourceTool -Destination $tool }
        }
    }
    # Do not change user keyboards, startup or shortcuts if UAC is cancelled.
    Invoke-HanBonGoUninstallStep 'Remove system keyboard registration' {
        $requestId = [guid]::NewGuid().ToString('N')
        $resultPath = Get-HanBonGoMachineResultPath $requestId
        $systemScript = Join-Path $PSScriptRoot 'uninstall.ps1'
        $principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
        $arguments = '-NoProfile -STA -ExecutionPolicy Bypass -File "' + $systemScript + '" -RequestId ' + $requestId
        $start = @{ FilePath = (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'); WindowStyle = 'Hidden'; ArgumentList = $arguments; PassThru = $true }
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { $start.Verb = 'RunAs' }
        $process = Start-Process @start
        $process.WaitForExit()
        if (-not (Test-Path -LiteralPath $resultPath)) { throw 'The administrator process did not return an uninstall result.' }
        $machineResult = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($machineResult.requestId -ne $requestId -or -not $machineResult.success -or $process.ExitCode -ne 0) {
            $exception = [InvalidOperationException]::new('System unregistration failed.')
            if ($machineResult.error) {
                $exception = [InvalidOperationException]::new([string]$machineResult.error.message)
                $exception.Data['HanBonGoStage'] = $machineResult.error.stage
                $exception.Data['HanBonGoMachineError'] = $machineResult.error
            }
            throw $exception
        }
    }
    # This phase always runs as the original user, including when UAC used
    # credentials belonging to a different administrator account.
    Remove-HanBonGoUserRegistration $tool $previousTip
    Invoke-HanBonGoUninstallStep 'Stop tray updater and remove startup entry' {
        $sessionId = (Get-Process -Id $PID).SessionId
        $running = @(Get-Process -Name 'HanBonGo.Updater' -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq $sessionId })
        $exitEvent = $null
        try {
            $exitEvent = [Threading.EventWaitHandle]::OpenExisting('Local\HanBonGoExit')
            [void]$exitEvent.Set()
        } catch [Threading.WaitHandleCannotBeOpenedException] {
            # No updater has created an exit event in this session.
        } finally { if ($exitEvent) { $exitEvent.Dispose() } }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        foreach ($updater in $running) {
            $remaining = [Math]::Max(0, 5000 - [int]$timer.ElapsedMilliseconds)
            if (-not $updater.WaitForExit($remaining)) { throw 'The tray updater did not exit; no process was forced to stop.' }
        }
        $runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
        if (Get-ItemProperty -LiteralPath $runKey -Name 'HanBonGoUpdater' -ErrorAction SilentlyContinue) { Remove-ItemProperty -LiteralPath $runKey -Name 'HanBonGoUpdater' }
    }
    Invoke-HanBonGoUninstallStep 'Remove Start menu shortcuts' {
        $startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) '한본어 입력기'
        foreach ($name in @('한본어 입력기 업데이터.lnk', '한본어 입력기 제거.lnk')) {
            $path = Join-Path $startMenu $name
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        if ((Test-Path -LiteralPath $startMenu) -and @(Get-ChildItem -LiteralPath $startMenu -Force).Count -eq 0) { Remove-Item -LiteralPath $startMenu }
        $startupLink = Join-Path ([Environment]::GetFolderPath('Startup')) '한본어 입력기 업데이터.lnk'
        if (Test-Path -LiteralPath $startupLink) { Remove-Item -LiteralPath $startupLink -Force }
    }
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show('한본어 입력기를 제거했습니다. 사용 중인 파일은 남아 있을 수 있습니다.', '한본어 입력기 제거')
} catch {
    $details = Get-HanBonGoUninstallError $_
    Add-Content -LiteralPath $logPath -Value ($details | ConvertTo-Json -Depth 8) -Encoding UTF8
    Add-Type -AssemblyName System.Windows.Forms
    $stageLabel = switch -Wildcard ($details.stage) {
        'Read default keyboard' { '기본 입력기 확인'; break }
        'Read installation information' { '설치 정보 확인'; break }
        'Read keyboard identity' { '입력기 식별 정보 확인'; break }
        'Prepare user keyboard removal' { '제거 준비'; break }
        'Obtain administrator privileges' { '관리자 권한 확인'; break }
        'Remove system keyboard registration' { '시스템 입력기 등록 해제'; break }
        'Unregister * keyboard' { '시스템 입력기 등록 해제'; break }
        'Verify system keyboard removal' { '시스템 입력기 제거 확인'; break }
        'Remove installed files' { '설치 파일 정리'; break }
        'Clear removed keyboard preference' { '삭제된 기본 입력기 정리'; break }
        'Preserve previous default keyboard' { '기본 입력기 복원'; break }
        '*keyboard*' { '사용자 키보드 목록 정리'; break }
        'Stop tray updater and remove startup entry' { '업데이터 및 시작프로그램 정리'; break }
        'Remove Start menu shortcuts' { '시작 메뉴 바로가기 정리'; break }
        default { '제거 처리' }
    }
    $message = '제거하지 못했습니다.' + "`r`n단계: " + $stageLabel + "`r`n" + $details.message + "`r`n`r`n오류 기록: " + $logPath
    [void][Windows.Forms.MessageBox]::Show($message, '한본어 입력기 제거')
    exit 1
} finally {
    $temporaryTool = Join-Path $workRoot 'ime-tool.exe'
    if (Test-Path -LiteralPath $temporaryTool) { Remove-Item -LiteralPath $temporaryTool -Force -ErrorAction SilentlyContinue }
    if ((Test-Path -LiteralPath $workRoot) -and @(Get-ChildItem -LiteralPath $workRoot -Force).Count -eq 0) { Remove-Item -LiteralPath $workRoot -ErrorAction SilentlyContinue }
}