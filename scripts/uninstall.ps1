param([string]$RequestId = ([guid]::NewGuid().ToString('N')))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'uninstall-common.ps1')
$resultPath = Get-HanBonGoMachineResultPath $RequestId
$result = $null
try {
    Invoke-HanBonGoUninstallStep 'Obtain administrator privileges' {
        $principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
    }
    $installRoot = [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'HanBonGo'))
    if (Test-Path -LiteralPath $installRoot) {
        $state = Invoke-HanBonGoUninstallStep 'Read installation information' { Get-Content -LiteralPath (Join-Path $installRoot 'active-install.json') -Raw | ConvertFrom-Json }
        $versionRoot = Assert-HanBonGoVersionPath $installRoot $state.versionRoot
        $identity = Invoke-HanBonGoUninstallStep 'Read keyboard identity' {
            $output = @(& (Join-Path $versionRoot 'x64\ime-tool.exe') --version 2>&1)
            if ($LASTEXITCODE -ne 0 -or $output.Count -lt 1 -or $output[0] -notmatch '(?i)^\{7b3b1841-a842-4bc5-[a-f0-9]{4}-[a-f0-9]{12}\}$') { throw 'Cannot read the installed HanBonGo keyboard identity.' }
            [string]$output[0]
        }
        foreach ($architecture in @('Win32', 'x64')) {
            $tool = Join-Path $versionRoot "$architecture\ime-tool.exe"
            $dll = Join-Path $versionRoot "$architecture\HangulJpIME.dll"
            if (Test-Path -LiteralPath $tool) {
                # TSF registration is shared. A second call may report an absent
                # profile; only verified absence below is treated as success.
                Invoke-HanBonGoUninstallStep "Unregister $architecture keyboard" {
                    $output = @(& $tool --unregister $dll 2>&1)
                    foreach ($line in $output) { Write-Host $line }
                }
            }
        }
        Invoke-HanBonGoUninstallStep 'Verify system keyboard removal' {
            foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
                $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
                try {
                    foreach ($path in @(('SOFTWARE\Classes\CLSID\' + $identity), ('SOFTWARE\Microsoft\CTF\TIP\' + $identity))) {
                        $key = $registry.OpenSubKey($path)
                        if ($key) { $key.Dispose(); throw "Keyboard registration remains in $view ($path)." }
                    }
                } finally { $registry.Dispose() }
            }
        }
        Invoke-HanBonGoUninstallStep 'Remove installed files' {
            $resolvedRoot = (Resolve-Path -LiteralPath $installRoot).Path
            if ($resolvedRoot -ne [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'HanBonGo'))) { throw 'Unexpected removal target.' }
            # Enumerate explicitly without following junctions; validate each
            # target before deletion. Retain files still mapped in an editor.
            $queue = [Collections.Generic.Queue[IO.DirectoryInfo]]::new()
            $queue.Enqueue([IO.DirectoryInfo]$resolvedRoot)
            $items = [Collections.Generic.List[IO.FileSystemInfo]]::new()
            while ($queue.Count -gt 0) {
                foreach ($item in $queue.Dequeue().GetFileSystemInfos()) {
                    $target = [IO.Path]::GetFullPath($item.FullName)
                    if (-not $target.StartsWith($resolvedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected removal child.' }
                    $items.Add($item)
                    if (($item.Attributes -band [IO.FileAttributes]::Directory) -and -not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { $queue.Enqueue([IO.DirectoryInfo]$item) }
                }
            }
            foreach ($item in ($items | Sort-Object { $_.FullName.Length } -Descending)) {
                try {
                    if ($item.Attributes -band [IO.FileAttributes]::Directory) { [IO.Directory]::Delete($item.FullName, $false) }
                    else { [IO.File]::Delete($item.FullName) }
                } catch { Write-Host "Retained in-use file or directory: $($item.FullName)" }
            }
            if (@(Get-ChildItem -LiteralPath $resolvedRoot -Force).Count -eq 0) { [IO.Directory]::Delete($resolvedRoot, $false) }
            else { Write-Host 'Loaded files remain; the input method is unregistered.' }
        }
    }
    $result = [pscustomobject]@{ requestId = $RequestId; success = $true }
} catch {
    $result = [pscustomobject]@{ requestId = $RequestId; success = $false; error = (Get-HanBonGoUninstallError $_ 'System unregistration') }
} finally {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resultPath) | Out-Null
    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resultPath -Encoding UTF8
}
if (-not $result.success) { Write-Host ($result.error | ConvertTo-Json -Depth 6 -Compress); exit 1 }