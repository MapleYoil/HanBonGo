$ErrorActionPreference = 'Stop'
# The account's International commands and native tools are never invoked.
& {
    . (Join-Path $PSScriptRoot 'uninstall-common.ps1')
    $testState = [pscustomobject]@{ Checks = 0 }
    $fake = [pscustomobject]@{ Languages = $null; Tip = ''; Native = ''; NativeCalls = 0; Writes = 0; PreferenceWrites = 0; FailWrite = $false; IgnoreWrite = $false; ResetDefault = $false }
    $han = '0411:{7B3B1841-A842-4BC5-1111-111111111111}{E97A77BE-9C2D-4E9D-81AD-6A2F76D9AB25}'
    $oldHan = '0411:{7b3b1841-a842-4bc5-2222-222222222222}{e97a77be-9c2d-4e9d-81ad-6a2f76d9ab25}'
    $lookalike = '0411:{7B3B1841-A842-4BC5-1111-111111111111}{11111111-2222-3333-4444-555555555555}'
    $ko = '0412:{A028AE76-01B1-46C2-99C4-ACD9858AE02F}{B5FE1F02-D5F2-4445-9C03-C568F23C99A1}'
    $ms = '0411:{03B5835F-F03C-411B-9CE2-AA23E1171E36}{A76C93D9-5523-4E90-AAFA-4DB112F9AC76}'
    $meltype = '0411:{417D801B-A9BD-4C26-BD16-356A825A6998}{21F643F4-72D5-4946-BF70-0A126AB52D05}'
    function Assert-Uninstall([bool]$Condition, [string]$Label) {
        if (-not $Condition) { throw "Uninstall test failed: $Label" }
        $testState.Checks++
    }
    function New-FakeLanguage([string]$Tag, [string[]]$Tips) {
        $list = [Collections.Generic.List[string]]::new()
        foreach ($tip in $Tips) { [void]$list.Add($tip) }
        [pscustomobject]@{ LanguageTag = $Tag; InputMethodTips = $list }
    }
    function Copy-FakeLanguages($Languages) {
        foreach ($language in $Languages) { New-FakeLanguage $language.LanguageTag @($language.InputMethodTips) }
    }
    function Reset-Fake([string]$Tip = $ko) {
        $fake.Languages = @((New-FakeLanguage 'ko-KR' @($ko)), (New-FakeLanguage 'ja-JP' @($ms, $han, $meltype, $oldHan, $lookalike)))
        $fake.Tip = $Tip; $fake.Native = 'remove'; $fake.NativeCalls = 0; $fake.Writes = 0; $fake.PreferenceWrites = 0
        $fake.FailWrite = $false; $fake.IgnoreWrite = $false; $fake.ResetDefault = $false
    }
    function Get-WinUserLanguageList { Copy-FakeLanguages $fake.Languages }
    function Set-WinUserLanguageList {
        param($LanguageList, [switch]$Force)
        $fake.Writes++
        if ($fake.FailWrite) { throw [Exception]::new('Simulated language-list failure.') }
        if (-not $fake.IgnoreWrite) { $fake.Languages = @(Copy-FakeLanguages $LanguageList) }
        if ($fake.ResetDefault) { $fake.Tip = $han }
    }
    function Get-WinDefaultInputMethodOverride { [pscustomobject]@{ InputMethodTip = $fake.Tip } }
    function Set-WinDefaultInputMethodOverride {
        param([string]$InputTip)
        $fake.PreferenceWrites++
        $fake.Tip = $InputTip
    }
    function Invoke-HanBonGoUninstallTool([string]$Tool, [string[]]$Arguments) {
        $fake.NativeCalls++
        if ($fake.Native -eq 'error') { throw [Exception]::new('Simulated native removal failure.') }
        if ($fake.Native -eq 'remove') {
            foreach ($language in $fake.Languages) {
                foreach ($tip in @($language.InputMethodTips)) {
                    if (Test-HanBonGoInputTip $tip) { [void]$language.InputMethodTips.Remove($tip) }
                }
            }
        }
    }
    function Assert-OtherKeyboards {
        Assert-Uninstall ($fake.Languages.Count -eq 2 -and $fake.Languages[0].LanguageTag -ceq 'ko-KR' -and $fake.Languages[1].LanguageTag -ceq 'ja-JP') 'language order remains Korean then Japanese'
        Assert-Uninstall (($fake.Languages[0].InputMethodTips -join '|') -ceq $ko) 'Korean keyboard retained'
        Assert-Uninstall (($fake.Languages[1].InputMethodTips -join '|') -ceq (@($ms, $meltype, $lookalike) -join '|')) 'Microsoft, Meltype and lookalike retain exact order'
    }
    Assert-Uninstall (Test-HanBonGoInputTip $han) 'current identity recognized'
    Assert-Uninstall (Test-HanBonGoInputTip $oldHan) 'old own release recognized case-insensitively'
    foreach ($tip in @($ko, $ms, $meltype, $lookalike, '', ($han + 'bad'))) { Assert-Uninstall (-not (Test-HanBonGoInputTip $tip)) 'non-product profile excluded' }

    Reset-Fake
    $output = @(Remove-HanBonGoUserRegistration 'fake-tool' $ko)
    Assert-Uninstall ($output.Count -eq 0) 'helper emits no language objects'
    Assert-Uninstall ($fake.NativeCalls -eq 1 -and $fake.Writes -eq 0) 'native success skips redundant language-list setter'
    Assert-Uninstall ($fake.Tip -ceq $ko -and $fake.PreferenceWrites -eq 0) 'existing explicit Korean default retained without writes'
    Assert-OtherKeyboards
    Remove-HanBonGoUserRegistration 'fake-tool' $ko
    Assert-Uninstall ($fake.NativeCalls -eq 1 -and $fake.Writes -eq 0) 'repeated removal is a no-op'

    foreach ($previous in @('', $ko, $meltype)) {
        Reset-Fake $previous
        $fake.Native = 'stale'; $fake.ResetDefault = $true
        Remove-HanBonGoUserRegistration 'fake-tool' $previous
        Assert-Uninstall ($fake.Writes -eq 1) 'stale owned entries rewrite the language list once'
        Assert-Uninstall ($fake.Tip -ceq $previous) 'language-list side effect restores previous default'
        Assert-OtherKeyboards
    }
    Reset-Fake $han
    Remove-HanBonGoUserRegistration 'fake-tool' $han
    Assert-Uninstall ([string]::IsNullOrEmpty($fake.Tip) -and $fake.PreferenceWrites -eq 1) 'removed default cleared rather than choosing a different keyboard'
    Assert-OtherKeyboards

    Reset-Fake
    $fake.Native = 'error'
    Remove-HanBonGoUserRegistration 'fake-tool' $ko
    Assert-Uninstall ($fake.NativeCalls -eq 1 -and $fake.Writes -eq 1) 'missing native profile succeeds only after exact cleanup readback'
    Assert-OtherKeyboards

    Reset-Fake
    Remove-HanBonGoUserRegistration '' $ko
    Assert-Uninstall ($fake.NativeCalls -eq 0 -and $fake.Writes -eq 1) 'partial installation with missing tool repairs stale list'
    Assert-OtherKeyboards

    Reset-Fake
    $fake.Native = 'stale'; $fake.FailWrite = $true
    $caught = $null
    try { Remove-HanBonGoUserRegistration 'fake-tool' $ko } catch { $caught = Get-HanBonGoUninstallError $_ }
    Assert-Uninstall ($null -ne $caught -and $caught.stage -eq 'Remove stale keyboard-list entries') 'language-list error identifies actual failing stage'
    Assert-Uninstall ($caught.message -eq 'Simulated language-list failure.' -and $caught.exception -match '0x' -and $caught.position -and $caught.stack) 'error includes exception type, HRESULT, position and stack'
    Assert-Uninstall ($fake.Tip -ceq $ko) 'failure preserves unrelated default'
    Assert-Uninstall (Test-HanBonGoUserRegistration $fake.Languages) 'failed setter does not claim removal'

    Reset-Fake
    $fake.Native = 'error'; $fake.IgnoreWrite = $true
    $caught = $null
    try { Remove-HanBonGoUserRegistration 'fake-tool' $ko } catch { $caught = Get-HanBonGoUninstallError $_ }
    Assert-Uninstall ($null -ne $caught -and $caught.message -eq 'Simulated native removal failure.') 'native error propagates when list cleanup is not saved'
    Reset-Fake
    $fake.Native = 'stale'; $fake.IgnoreWrite = $true
    $caught = $null
    try { Remove-HanBonGoUserRegistration 'fake-tool' $ko } catch { $caught = $_ }
    Assert-Uninstall ($null -ne $caught) 'readback rejects ignored list update'

    $root = 'C:\Program Files\HanBonGo'
    Assert-Uninstall ((Assert-HanBonGoVersionPath $root ($root + '\123456789abc')) -ceq ($root + '\123456789abc')) 'direct version path accepted'
    foreach ($path in @($root, 'C:\Windows', ($root + '-other\123'), ($root + '\..\Elsewhere'), ($root + '\version\child'))) {
        $threw = $false; try { [void](Assert-HanBonGoVersionPath $root $path) } catch { $threw = $true }
        Assert-Uninstall $threw 'invalid or non-direct removal target rejected'
    }
    $request = '11111111222233334444555555555555'
    Assert-Uninstall ((Get-HanBonGoMachineResultPath $request).EndsWith(('uninstall-system-' + $request + '.json'))) 'machine result is tied to one request'
    $threw = $false; try { [void](Get-HanBonGoMachineResultPath '..\invalid') } catch { $threw = $true }
    Assert-Uninstall $threw 'untrusted result filename rejected'

    # Parse both entry points without executing account changes, UAC or dialogs.
    foreach ($name in @('uninstall-common.ps1', 'uninstall.ps1', 'uninstall-launcher.ps1')) {
        $tokens = $null; $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name), [ref]$tokens, [ref]$errors)
        Assert-Uninstall ($errors.Count -eq 0) "$name parses in the running PowerShell version"
    }
    $launcher = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'uninstall-launcher.ps1') -Raw
    Assert-Uninstall ($launcher.IndexOf("'Remove system keyboard registration'") -lt $launcher.IndexOf('Remove-HanBonGoUserRegistration')) 'administrator success precedes user mutations'
    $bytes = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'uninstall-launcher.ps1'))
    Assert-Uninstall ($bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) 'Korean shortcut script uses UTF-8 BOM for Windows PowerShell 5.1'
    Write-Host "Uninstall PASS ($($testState.Checks) checks; account settings and installation untouched)"
}