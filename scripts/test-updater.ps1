$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'update-functions.ps1')
$checks = 0
function Check([bool]$Condition) { if (-not $Condition) { throw "Updater test failed at check $checks" }; $script:checks++ }
$valid = [pscustomobject]@{ draft = $false; prerelease = $false; tag_name = 'v0.2.0'; assets = @([pscustomobject]@{ name = 'HanBonGo-windows.zip'; browser_download_url = 'https://github.com/MapleYoil/HanBonGo/releases/download/v0.2.0/HanBonGo-windows.zip'; digest = 'sha256:' + ('a' * 64); size = 1000 }) }
Check ($null -ne (Get-HanBonGoAsset $valid '0.1.0'))
Check ($null -eq (Get-HanBonGoAsset $valid '0.2.0'))
Check ($null -eq (Get-HanBonGoAsset $valid '0.3.0'))
$valid.prerelease = $true; Check ($null -eq (Get-HanBonGoAsset $valid '0.1.0')); $valid.prerelease = $false
foreach ($invalidUrl in @('http://github.com/MapleYoil/HanBonGo/releases/download/v0.2.0/HanBonGo-windows.zip', 'https://github.com/Other/Repo/releases/download/v0.2.0/HanBonGo-windows.zip', 'https://evil.example/HanBonGo-windows.zip')) {
    $valid.assets[0].browser_download_url = $invalidUrl
    $rejected = $false; try { Get-HanBonGoAsset $valid '0.1.0' | Out-Null } catch { $rejected = $true }; Check $rejected
}
$valid.assets[0].browser_download_url = 'https://github.com/MapleYoil/HanBonGo/releases/download/v0.2.0/HanBonGo-windows.zip'
$valid.assets[0].digest = 'sha256:bad'; $rejected = $false; try { Get-HanBonGoAsset $valid '0.1.0' | Out-Null } catch { $rejected = $true }; Check $rejected
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('HanBonGo-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
foreach ($entryName in @('../escape.txt', 'scripts/../../escape.txt', 'C:/escape.txt')) {
    $archive = Join-Path $testRoot ([guid]::NewGuid().ToString('N') + '.zip')
    $zip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
    [void]$zip.CreateEntry($entryName); $zip.Dispose()
    $rejected = $false; try { Test-HanBonGoArchive $archive (Join-Path $testRoot 'extracted') } catch { $rejected = $true }; Check $rejected
}
foreach ($testFile in Get-ChildItem -LiteralPath $testRoot -File) { Remove-Item -LiteralPath $testFile.FullName }
Remove-Item -LiteralPath $testRoot
Write-Host "Updater PASS ($checks checks)"
