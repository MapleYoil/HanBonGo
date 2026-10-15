$ErrorActionPreference = 'Stop'

# Keep command mocks inside a child scope; this file never calls the account's
# International-module getters or setters, even when dot-sourced by a caller.
& {
    . (Join-Path $PSScriptRoot 'input-preferences.ps1')
    $testState = [pscustomobject]@{ Checks = 0 }
    $fakePreference = [pscustomobject]@{
        Tip = ''; Writes = [Collections.Generic.List[object]]::new()
        FailSetter = $false; IgnoreSetter = $false; NullWhenEmpty = $false; Reads = 0
    }
    function Assert-PreferenceTest([bool]$Condition, [string]$Label) {
        if (-not $Condition) { throw "Input preference test failed: $Label" }
        $testState.Checks++
    }
    function Assert-Tips($Actual, [string[]]$Expected, [string]$Label) {
        Assert-PreferenceTest ($Actual.Count -eq $Expected.Count) "$Label count"
        for ($index = 0; $index -lt $Expected.Count; $index++) {
            Assert-PreferenceTest ($Actual[$index] -ceq $Expected[$index]) "$Label item $index"
        }
    }
    function New-TestLanguage([string]$Tag, [string[]]$Tips) {
        $typedTips = [Collections.Generic.List[string]]::new()
        foreach ($tip in $Tips) { [void]$typedTips.Add($tip) }
        [pscustomobject]@{ LanguageTag = $Tag; InputMethodTips = $typedTips }
    }
    function Get-WinDefaultInputMethodOverride {
        $fakePreference.Reads++
        if ($fakePreference.NullWhenEmpty -and [string]::IsNullOrEmpty($fakePreference.Tip)) { return $null }
        [pscustomobject]@{ InputMethodTip = $fakePreference.Tip }
    }
    function Set-WinDefaultInputMethodOverride {
        param([string]$InputTip)
        [void]$fakePreference.Writes.Add([pscustomobject]@{
            HasInputTip = $PSBoundParameters.ContainsKey('InputTip'); Tip = $InputTip
        })
        if ($fakePreference.FailSetter) { throw 'Simulated preference write failure.' }
        if (-not $fakePreference.IgnoreSetter) { $fakePreference.Tip = $InputTip }
    }
    function Reset-FakePreference([string]$CurrentTip) {
        $fakePreference.Tip = $CurrentTip
        $fakePreference.Writes.Clear()
        $fakePreference.FailSetter = $false
        $fakePreference.IgnoreSetter = $false
        $fakePreference.NullWhenEmpty = $false
        $fakePreference.Reads = 0
    }

    $oldHan = '0411:{7B3B1841-A842-4BC5-1111-111111111111}{E97A77BE-9C2D-4E9D-81AD-6A2F76D9AB25}'
    $duplicateHan = '0411:{7b3b1841-a842-4bc5-2222-222222222222}{e97a77be-9c2d-4e9d-81ad-6a2f76d9ab25}'
    $newHan = '0411:{7B3B1841-A842-4BC5-3333-333333333333}{E97A77BE-9C2D-4E9D-3333-333333333333}'
    $microsoftJapanese = '0411:{03B5835F-F03C-411B-9CE2-AA23E1171E36}{A76C93D9-5523-4E90-AAFA-4DB112F9AC76}'
    $meltype = '0411:{417D801B-A9BD-4C26-BD16-356A825A6998}{21F643F4-72D5-4946-BF70-0A126AB52D05}'
    $korean = '0412:{A028AE76-01B1-46C2-99C4-ACD9858AE02F}{B5FE1F02-D5F2-4445-9C03-C568F23C99A1}'
    $lookalike = '0411:{7B3B1841-A842-4BC5-1111-111111111111}{11111111-2222-3333-4444-555555555555}'

    # Replacing a release identity preserves its position and the typed list
    # instance, as well as the order and identities of the other languages.
    $languages = [Collections.Generic.List[object]]::new()
    $ko = New-TestLanguage 'ko-KR' @($korean, '0412:00000412')
    $ja = New-TestLanguage 'ja-JP' @($microsoftJapanese, $oldHan, $meltype, $lookalike)
    $en = New-TestLanguage 'en-US' @('0409:00000409')
    foreach ($language in @($ko, $ja, $en)) { [void]$languages.Add($language) }
    $originalTypedList = $ja.InputMethodTips
    $output = @(Update-HanBonGoKeyboardList $languages $newHan)
    Assert-PreferenceTest ($output.Count -eq 0) 'keyboard helper emits no collection items'
    Assert-PreferenceTest ($ja.InputMethodTips -is [Collections.Generic.List[string]]) 'keyboard tips remain a typed string list'
    Assert-PreferenceTest ([object]::ReferenceEquals($originalTypedList, $ja.InputMethodTips)) 'keyboard tip list instance preserved'
    Assert-Tips $ja.InputMethodTips @($microsoftJapanese, $newHan, $meltype, $lookalike) 'replace at original position'
    Assert-PreferenceTest ($languages.Count -eq 3) 'language count unchanged'
    Assert-PreferenceTest ([object]::ReferenceEquals($languages[0], $ko) -and [object]::ReferenceEquals($languages[1], $ja) -and [object]::ReferenceEquals($languages[2], $en)) 'language order and instances preserved'
    Assert-Tips $ko.InputMethodTips @($korean, '0412:00000412') 'Korean keyboards unchanged'
    Assert-Tips $en.InputMethodTips @('0409:00000409') 'English keyboards unchanged'

    $duplicates = New-TestLanguage 'ja' @($oldHan, $meltype, $duplicateHan, $newHan, $microsoftJapanese)
    Update-HanBonGoKeyboardList @($duplicates) $newHan
    Assert-Tips $duplicates.InputMethodTips @($newHan, $meltype, $microsoftJapanese) 'duplicate releases collapse at first position'
    Update-HanBonGoKeyboardList @($duplicates) $newHan
    Assert-Tips $duplicates.InputMethodTips @($newHan, $meltype, $microsoftJapanese) 'reinstall is idempotent'

    $firstInstall = New-TestLanguage 'ja-JP' @($meltype, $microsoftJapanese)
    Update-HanBonGoKeyboardList @($firstInstall) $newHan
    Assert-Tips $firstInstall.InputMethodTips @($meltype, $microsoftJapanese, $newHan) 'first install appends without reordering'
    $emptyJapanese = New-TestLanguage 'ja' @()
    Update-HanBonGoKeyboardList @($emptyJapanese) $newHan
    Assert-Tips $emptyJapanese.InputMethodTips @($newHan) 'empty Japanese keyboard list'
    $missingJapaneseThrew = $false
    try { Update-HanBonGoKeyboardList @($ko, $en) $newHan } catch { $missingJapaneseThrew = $true }
    Assert-PreferenceTest $missingJapaneseThrew 'missing Japanese support is reported'
    Assert-Tips $ko.InputMethodTips @($korean, '0412:00000412') 'failed registration preserves other language'

    foreach ($previous in @('', $korean, $meltype)) {
        Reset-FakePreference $previous
        $output = @(Restore-HanBonGoInputPreference $previous $newHan)
        Assert-PreferenceTest ($fakePreference.Tip -ceq $previous) 'existing default preference preserved'
        Assert-PreferenceTest ($fakePreference.Writes.Count -eq 0) 'identical preference has no setter call'
        Assert-PreferenceTest ($output.Count -eq 0) 'preference helper emits no command objects'
        Assert-PreferenceTest ($fakePreference.Reads -eq 2) 'preference checked before and after'
    }

    Reset-FakePreference ''
    $fakePreference.NullWhenEmpty = $true
    Restore-HanBonGoInputPreference '' $newHan
    Assert-PreferenceTest ($fakePreference.Writes.Count -eq 0) 'null empty override needs no write'

    # Model Set-WinUserLanguageList resetting an explicit preference. Restoring
    # it must use the snapshot, rather than make HanBonGo the new default.
    foreach ($previous in @($korean, $meltype)) {
        Reset-FakePreference $newHan
        Restore-HanBonGoInputPreference $previous $newHan
        Assert-PreferenceTest ($fakePreference.Tip -ceq $previous) 'language-list reset repairs previous preference'
        Assert-PreferenceTest ($fakePreference.Writes.Count -eq 1) 'changed explicit preference written once'
        Assert-PreferenceTest ($fakePreference.Writes[0].HasInputTip -and $fakePreference.Writes[0].Tip -ceq $previous) 'explicit preference setter receives previous tip'
    }
    Reset-FakePreference $korean
    Restore-HanBonGoInputPreference '' $newHan
    Assert-PreferenceTest ([string]::IsNullOrEmpty($fakePreference.Tip)) 'language-list reset restores unset override'
    Assert-PreferenceTest ($fakePreference.Writes.Count -eq 1 -and -not $fakePreference.Writes[0].HasInputTip) 'clear override calls setter without InputTip'

    Reset-FakePreference $oldHan
    Restore-HanBonGoInputPreference $oldHan $newHan
    Assert-PreferenceTest ($fakePreference.Tip -ceq $newHan) 'existing explicit HanBonGo preference migrates release identity'
    Assert-PreferenceTest ($fakePreference.Writes.Count -eq 1 -and $fakePreference.Writes[0].Tip -ceq $newHan) 'migrated preference uses new exact tip'
    Reset-FakePreference $newHan
    Restore-HanBonGoInputPreference $oldHan $newHan
    Assert-PreferenceTest ($fakePreference.Writes.Count -eq 0) 'already migrated HanBonGo preference has no redundant write'
    Reset-FakePreference $korean
    Restore-HanBonGoInputPreference $lookalike $newHan
    Assert-PreferenceTest ($fakePreference.Tip -ceq $lookalike) 'unrelated profile sharing class prefix is preserved'

    Reset-FakePreference $newHan
    $fakePreference.FailSetter = $true
    $failedWriteThrew = $false
    try { Restore-HanBonGoInputPreference $korean $newHan } catch { $failedWriteThrew = $_.Exception.Message -eq 'Simulated preference write failure.' }
    Assert-PreferenceTest $failedWriteThrew 'setter error propagates'
    Assert-PreferenceTest ($fakePreference.Tip -ceq $newHan) 'failed setter does not pretend preference restored'
    Reset-FakePreference $newHan
    $fakePreference.IgnoreSetter = $true
    $ignoredWriteThrew = $false
    try { Restore-HanBonGoInputPreference $korean $newHan } catch { $ignoredWriteThrew = $_.Exception.Message -eq 'Windows did not preserve the input-method preference.' }
    Assert-PreferenceTest $ignoredWriteThrew 'unsaved preference is detected by readback'

    Write-Host "Input preferences PASS ($($testState.Checks) checks; account settings untouched)"
}
