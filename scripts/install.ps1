$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Windows TSF system registration requires an administrator PowerShell. Run this script as administrator.'
}
$installRoot = Join-Path $env:ProgramFiles 'HanBonGo'
Start-Transcript -Path (Join-Path $projectRoot 'install.log') -Force | Out-Null
$releaseHashes = @((Get-FileHash -LiteralPath (Join-Path $projectRoot 'build\x64\Release\HangulJpIME.dll')).Hash, (Get-FileHash -LiteralPath (Join-Path $projectRoot 'build\updater\HanBonGo.Updater.exe')).Hash)
$sha = [Security.Cryptography.SHA256]::Create()
try { $releaseHash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($releaseHashes -join ':'))))).Replace('-', '').Substring(0, 12).ToLowerInvariant() } finally { $sha.Dispose() }
$previousVersion = $null
$statePath = Join-Path $installRoot 'active-install.json'
if (Test-Path -LiteralPath $statePath) { $previousVersion = (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json).versionRoot }
elseif (Test-Path -LiteralPath (Join-Path $env:ProgramFiles 'HangulJpIME\active-install.json')) { $previousVersion = (Get-Content -LiteralPath (Join-Path $env:ProgramFiles 'HangulJpIME\active-install.json') -Raw | ConvertFrom-Json).versionRoot }
$versionRoot = Join-Path $installRoot $releaseHash
foreach ($architecture in @('x64', 'Win32')) {
    $source = Join-Path $projectRoot "build\$architecture\Release"
    foreach ($file in @('HangulJpIME.dll', 'ime-tool.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $source $file))) { throw "Run build.ps1 first: $source" }
    }
}
foreach ($architecture in @('x64', 'Win32')) {
    $destination = Join-Path $versionRoot $architecture
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    foreach ($file in @('HangulJpIME.dll', 'ime-tool.exe')) {
        $sourceFile = Join-Path $projectRoot "build\$architecture\Release\$file"
        $destinationFile = Join-Path $destination $file
        if ((Test-Path -LiteralPath $destinationFile) -and (Get-FileHash -LiteralPath $sourceFile).Hash -eq (Get-FileHash -LiteralPath $destinationFile).Hash) { continue }
        Copy-Item -LiteralPath $sourceFile -Destination $destination
    }
    & (Join-Path $destination 'ime-tool.exe') --register (Join-Path $destination 'HangulJpIME.dll')
    if ($LASTEXITCODE -ne 0) { throw "Registration failed: $architecture" }
}
if ($previousVersion -and $previousVersion -ne $versionRoot) {
    # A previous native COM identity can remain cached safely, but its keyboard
    # profile must disappear from the user's input-method list.
    $oldIdentity = @(& (Join-Path $previousVersion 'x64\ime-tool.exe') --version)
    $newIdentity = @(& (Join-Path $versionRoot 'x64\ime-tool.exe') --version)
    if ($oldIdentity[0] -ne $newIdentity[0]) {
        foreach ($architecture in @('Win32', 'x64')) {
            $previousTool = Join-Path $previousVersion "$architecture\ime-tool.exe"
            if (Test-Path -LiteralPath $previousTool) {
                & $previousTool --unregister (Join-Path $previousVersion "$architecture\HangulJpIME.dll")
                # Registration is shared between architectures; the second
                # call can report a profile already removed by the first.
            }
        }
    }
}
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'version.json') -Raw | ConvertFrom-Json
$updaterDestination = Join-Path $versionRoot 'updater'
New-Item -ItemType Directory -Force -Path $updaterDestination | Out-Null
foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build\updater') -File | Where-Object { $_.Name -match '^HanBonGo\.Updater\.exe' })) {
    $destinationFile = Join-Path $updaterDestination $file.Name
    if ((Test-Path -LiteralPath $destinationFile) -and (Get-FileHash -LiteralPath $file.FullName).Hash -eq (Get-FileHash -LiteralPath $destinationFile).Hash) { continue }
    Copy-Item -LiteralPath $file.FullName -Destination $updaterDestination -Force
}
$managementRoot = Join-Path $installRoot 'management'
New-Item -ItemType Directory -Force -Path $managementRoot | Out-Null
$obsoleteUpdater = Join-Path $managementRoot 'updater.ps1'
if (Test-Path -LiteralPath $obsoleteUpdater) { Remove-Item -LiteralPath $obsoleteUpdater -Force }
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $managementRoot -Force }
Copy-Item -LiteralPath (Join-Path $projectRoot 'version.json') -Destination $installRoot -Force
@{ versionRoot = $versionRoot; installedAt = (Get-Date).ToString('o'); version = $manifest.version; repository = 'MapleYoil/HanBonGo' } | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
Write-Host 'Installed HanBonGo IME.'
& (Join-Path $PSScriptRoot 'register-shortcuts.ps1')
Stop-Transcript | Out-Null
