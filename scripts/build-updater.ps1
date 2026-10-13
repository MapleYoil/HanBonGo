$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $projectRoot 'build\updater'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$compiler = Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$references = @('System.dll', 'System.Core.dll', 'System.Drawing.dll', 'System.Windows.Forms.dll', 'System.Web.Extensions.dll', 'System.Net.Http.dll', 'System.IO.Compression.dll', 'System.IO.Compression.FileSystem.dll')
$arguments = @('/nologo', '/target:winexe', '/platform:anycpu', '/optimize+', '/utf8output', ('/out:' + (Join-Path $output 'HanBonGo.Updater.exe')))
$arguments += '/win32icon:' + (Join-Path $projectRoot 'src\resources\HanBonGo.ico')
foreach ($reference in $references) { $arguments += '/reference:' + $reference }
& $compiler @arguments (Join-Path $projectRoot 'updater\TrayUpdater.cs')
if ($LASTEXITCODE -ne 0) { throw 'Tray updater build failed.' }
$report = Join-Path $output 'self-test.json'
$testProcess = Start-Process -FilePath (Join-Path $output 'HanBonGo.Updater.exe') -WindowStyle Hidden -ArgumentList ('--self-test "' + $report + '"') -PassThru
$testProcess.WaitForExit()
if ($testProcess.ExitCode -ne 0) { throw "Tray updater tests failed: $report" }
Get-Content -LiteralPath $report
