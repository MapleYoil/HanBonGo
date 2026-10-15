function Update-HanBonGoKeyboardList($Languages, [string]$InputTip) {
    $japanese = $Languages | Where-Object { $_.LanguageTag -eq 'ja' -or $_.LanguageTag -eq 'ja-JP' } | Select-Object -First 1
    if (-not $japanese) { throw 'Japanese language support is required.' }
    $replacement = [Collections.Generic.List[string]]::new()
    $inserted = $false
    foreach ($tip in @($japanese.InputMethodTips)) {
        if ($tip -match '(?i)^0411:\{7b3b1841-a842-4bc5-[a-f0-9]{4}-[a-f0-9]{12}\}\{e97a77be-9c2d-4e9d-[a-f0-9]{4}-[a-f0-9]{12}\}$') {
            if (-not $inserted) { $replacement.Add($InputTip); $inserted = $true }
        } else { $replacement.Add($tip) }
    }
    if (-not $inserted) { $replacement.Add($InputTip) }
    $japanese.InputMethodTips.Clear()
    foreach ($tip in $replacement) { [void]$japanese.InputMethodTips.Add($tip) }
}

function Restore-HanBonGoInputPreference([AllowEmptyString()][string]$PreviousTip, [string]$InputTip) {
    $desired = $PreviousTip
    # Only migrate an existing explicit preference for our replaced COM identity.
    if ($desired -match '(?i)^0411:\{7b3b1841-a842-4bc5-[a-f0-9]{4}-[a-f0-9]{12}\}\{e97a77be-9c2d-4e9d-[a-f0-9]{4}-[a-f0-9]{12}\}$') { $desired = $InputTip }
    $actual = [string](Get-WinDefaultInputMethodOverride).InputMethodTip
    if ($actual -ne $desired) {
        if ([string]::IsNullOrEmpty($desired)) { Set-WinDefaultInputMethodOverride }
        else { Set-WinDefaultInputMethodOverride -InputTip $desired }
    }
    if ([string](Get-WinDefaultInputMethodOverride).InputMethodTip -ne $desired) { throw 'Windows did not preserve the input-method preference.' }
}
