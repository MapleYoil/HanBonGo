param([switch]$SkipInstall)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'generate-icons.ps1')
foreach ($architecture in @('x64', 'Win32')) {
    $buildPath = Join-Path $projectRoot "build\$architecture"
    & cmake -S $projectRoot -B $buildPath -G 'Visual Studio 17 2022' -A $architecture
    if ($LASTEXITCODE -ne 0) { throw "Configure failed: $architecture" }
    & cmake --build $buildPath --config Release
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $architecture" }
    & ctest --test-dir $buildPath -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed: $architecture" }
}
& (Join-Path $PSScriptRoot 'build-updater.ps1')
& (Join-Path $PSScriptRoot 'test-updater.ps1')
& (Join-Path $PSScriptRoot 'package.ps1')
if (-not $SkipInstall) { & (Join-Path $PSScriptRoot 'update.ps1') }
