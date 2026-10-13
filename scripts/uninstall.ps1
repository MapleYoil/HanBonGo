$ErrorActionPreference = 'Stop'
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
$installRoot = [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'HanBonGo'))
$state = Get-Content -LiteralPath (Join-Path $installRoot 'active-install.json') -Raw | ConvertFrom-Json
$versionRoot = [IO.Path]::GetFullPath($state.versionRoot)
if (-not $versionRoot.StartsWith($installRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid installation path.' }
$identity = @(& (Join-Path $versionRoot 'x64\ime-tool.exe') --version)[0]
foreach ($architecture in @('Win32', 'x64')) {
    $tool = Join-Path $versionRoot "$architecture\ime-tool.exe"
    $dll = Join-Path $versionRoot "$architecture\HangulJpIME.dll"
    if (Test-Path -LiteralPath $tool) {
        & $tool --unregister $dll
    }
}
foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
    $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
    try {
        $key = $registry.OpenSubKey('SOFTWARE\Classes\CLSID\' + $identity)
        if ($key) { $key.Dispose(); throw "COM registration remains in $view" }
    } finally { $registry.Dispose() }
}
# Delete only the explicitly validated product directory, children first.
# Retain DLLs still mapped in an editor rather than terminating that editor.
$resolvedRoot = (Resolve-Path -LiteralPath $installRoot).Path
if ($resolvedRoot -ne [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'HanBonGo'))) { throw 'Unexpected removal target.' }
foreach ($item in (Get-ChildItem -LiteralPath $resolvedRoot -Recurse -Force | Sort-Object { $_.FullName.Length } -Descending)) {
    if ($item.PSIsContainer -and @(Get-ChildItem -LiteralPath $item.FullName -Force).Count -gt 0) { continue }
    try { Remove-Item -LiteralPath $item.FullName -Force -ErrorAction Stop } catch { Write-Host "Retained in-use file: $($item.FullName)" }
}
if (@(Get-ChildItem -LiteralPath $resolvedRoot -Force).Count -eq 0) { Remove-Item -LiteralPath $resolvedRoot -Force }
else { Write-Host 'Loaded files remain; the input method is unregistered.' }
