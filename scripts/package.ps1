$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $projectRoot ('dist\stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($architecture in @('x64', 'Win32')) {
    $destination = Join-Path $stage "build\$architecture\Release"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    foreach ($file in @('HangulJpIME.dll', 'ime-tool.exe')) { Copy-Item -LiteralPath (Join-Path $projectRoot "build\$architecture\Release\$file") -Destination $destination }
}
Copy-Item -LiteralPath $PSScriptRoot -Destination (Join-Path $stage 'scripts') -Recurse
$updaterStage = Join-Path $stage 'build\updater'
New-Item -ItemType Directory -Force -Path $updaterStage | Out-Null
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build\updater') -Filter 'HanBonGo.Updater.exe*' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $updaterStage }
Copy-Item -LiteralPath (Join-Path $projectRoot 'version.json'), (Join-Path $projectRoot 'README.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $projectRoot '설치.cmd') -Destination $stage
$licenseStage = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Force -Path $licenseStage | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'src\resources\fonts\LICENSE.txt') -Destination (Join-Path $licenseStage 'PretendardJP-LICENSE.txt')
$archive = Join-Path $projectRoot 'dist\HanBonGo-windows.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Write-Host "Release package prepared locally: $archive"
