function Test-HanBonGoInputTip([AllowEmptyString()][string]$InputTip) {
    return $InputTip -match '(?i)^0411:\{7b3b1841-a842-4bc5-[a-f0-9]{4}-[a-f0-9]{12}\}\{e97a77be-9c2d-4e9d-[a-f0-9]{4}-[a-f0-9]{12}\}$'
}

function Assert-HanBonGoVersionPath([string]$InstallRoot, [string]$VersionRoot) {
    $root = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
    $version = [IO.Path]::GetFullPath($VersionRoot).TrimEnd('\')
    if (-not $version.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetDirectoryName($version) -ne $root) { throw 'Invalid installation version path.' }
    return $version
}

function Invoke-HanBonGoUninstallStep([string]$Stage, [scriptblock]$Action) {
    try { & $Action } catch {
        if (-not $_.Exception.Data['HanBonGoStage']) { $_.Exception.Data['HanBonGoStage'] = $Stage }
        throw
    }
}

function Get-HanBonGoUninstallError($Record, [string]$FallbackStage = 'Uninstall') {
    $stage = [string]$Record.Exception.Data['HanBonGoStage']
    if ([string]::IsNullOrEmpty($stage)) { $stage = $FallbackStage }
    $causes = @(); $cause = $Record.Exception
    while ($cause) { $causes += ('{0}: {1} (0x{2:X8})' -f $cause.GetType().FullName, $cause.Message, $cause.HResult); $cause = $cause.InnerException }
    [pscustomobject]@{
        timestamp = (Get-Date).ToString('o'); stage = $stage
        message = $Record.Exception.Message; exception = $causes -join "`r`n"
        errorId = $Record.FullyQualifiedErrorId; category = [string]$Record.CategoryInfo
        position = $Record.InvocationInfo.PositionMessage; stack = $Record.ScriptStackTrace
        powershell = $PSVersionTable.PSVersion.ToString()
        machine = $Record.Exception.Data['HanBonGoMachineError']
    }
}

function Invoke-HanBonGoUninstallTool([string]$Tool, [string[]]$Arguments) {
    $output = @(& $Tool @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) { throw ('{0} {1} exited {2}. {3}' -f $Tool, ($Arguments -join ' '), $exitCode, ($output -join "`n")) }
    foreach ($line in $output) { Write-Host $line }
}

function Test-HanBonGoUserRegistration($Languages) {
    foreach ($language in $Languages) {
        foreach ($tip in $language.InputMethodTips) {
            if (Test-HanBonGoInputTip $tip) { return $true }
        }
    }
    return $false
}

function Remove-HanBonGoUserRegistration([AllowEmptyString()][string]$Tool, [AllowEmptyString()][string]$PreviousTip) {
    $previousTip = $PreviousTip
    $removedDefault = Test-HanBonGoInputTip $previousTip
    try {
        # An explicit preference must stop referring to the removed keyboard.
        # Clearing it lets Windows retain the user's language order.
        if ($removedDefault) { Invoke-HanBonGoUninstallStep 'Clear removed keyboard preference' { Set-WinDefaultInputMethodOverride } }
        $languages = Invoke-HanBonGoUninstallStep 'Read remaining keyboards' { Get-WinUserLanguageList }
        $nativeError = $null
        if ((Test-HanBonGoUserRegistration $languages) -and -not [string]::IsNullOrEmpty($Tool)) {
            try {
                Invoke-HanBonGoUninstallStep 'Remove user keyboard registration (InstallLayoutOrTip)' {
                    Invoke-HanBonGoUninstallTool $Tool @('--user-uninstall')
                }
            } catch {
                # The administrator phase may already have removed this profile.
                # Accept a native error only after exact cleanup and readback
                # prove that no HanBonGo entry remains in this account.
                $nativeError = $_
            }
        }
        $languages = Invoke-HanBonGoUninstallStep 'Read remaining keyboards' { Get-WinUserLanguageList }
        $changed = $false
        foreach ($language in $languages) {
            foreach ($tip in @($language.InputMethodTips)) {
                if (Test-HanBonGoInputTip $tip) { [void]$language.InputMethodTips.Remove($tip); $changed = $true }
            }
        }
        # InstallLayoutOrTip normally updates the list itself. Rewriting the
        # entire Windows language list a second time is unnecessary and can
        # fail while Windows is processing the native removal notification.
        if ($changed) {
            Invoke-HanBonGoUninstallStep 'Remove stale keyboard-list entries' { Set-WinUserLanguageList -LanguageList $languages -Force }
        }
        $remaining = Invoke-HanBonGoUninstallStep 'Verify user keyboard removal' { Get-WinUserLanguageList }
        foreach ($language in $remaining) {
            foreach ($tip in $language.InputMethodTips) {
                if (Test-HanBonGoInputTip $tip) {
                    if ($nativeError) { throw $nativeError }
                    throw 'HanBonGo is still present in the Windows keyboard list.'
                }
            }
        }
    } finally {
        if ($removedDefault) {
            Invoke-HanBonGoUninstallStep 'Clear removed keyboard preference' {
                if (Test-HanBonGoInputTip ([string](Get-WinDefaultInputMethodOverride).InputMethodTip)) { Set-WinDefaultInputMethodOverride }
            }
        }
        if (-not $removedDefault) {
            Invoke-HanBonGoUninstallStep 'Preserve previous default keyboard' {
                $actual = [string](Get-WinDefaultInputMethodOverride).InputMethodTip
                if ($actual -ne $previousTip) {
                    if ([string]::IsNullOrEmpty($previousTip)) { Set-WinDefaultInputMethodOverride }
                    else { Set-WinDefaultInputMethodOverride -InputTip $previousTip }
                }
                if ([string](Get-WinDefaultInputMethodOverride).InputMethodTip -ne $previousTip) { throw 'Windows did not preserve the previous default keyboard.' }
            }
        }
    }
}

function Get-HanBonGoMachineResultPath([string]$RequestId) {
    $parsed = [guid]::Empty
    if (-not [guid]::TryParseExact($RequestId, 'N', [ref]$parsed)) { throw 'Invalid uninstall request ID.' }
    return Join-Path (Join-Path $env:ProgramData 'HanBonGo') ('uninstall-system-' + $RequestId + '.json')
}
