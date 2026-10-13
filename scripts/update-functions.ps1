function Get-HanBonGoVersion([string]$Text) {
    if ($Text -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$') { throw 'Only numeric stable release versions are supported.' }
    return [version]($Text.TrimStart('v'))
}
function Get-HanBonGoAsset($Release, [string]$CurrentVersion) {
    if ($Release.draft -or $Release.prerelease) { return $null }
    if ((Get-HanBonGoVersion $Release.tag_name) -le (Get-HanBonGoVersion $CurrentVersion)) { return $null }
    $assets = @($Release.assets | Where-Object name -eq 'HanBonGo-windows.zip')
    if ($assets.Count -ne 1) { throw 'Release must contain exactly one HanBonGo-windows.zip.' }
    $asset = $assets[0]
    $uri = [uri]$asset.browser_download_url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or $uri.AbsolutePath -notlike '/MapleYoil/HanBonGo/releases/download/*/HanBonGo-windows.zip') { throw 'Unexpected release download URL.' }
    if ($asset.digest -notmatch '^sha256:[a-fA-F0-9]{64}$') { throw 'Release asset is missing a SHA-256 digest.' }
    if ($asset.size -le 0 -or $asset.size -gt 52428800) { throw 'Release package size is invalid.' }
    return $asset
}
function Test-HanBonGoArchive([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $root = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $total = 0L
        foreach ($entry in $zip.Entries) {
            if ((($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw 'Symbolic links are not allowed in an update package.' }
            $target = [IO.Path]::GetFullPath((Join-Path $root $entry.FullName))
            if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or $entry.FullName.Contains(':')) { throw 'Unsafe package path.' }
            $total += $entry.Length
            if ($total -gt 104857600) { throw 'Expanded release package is too large.' }
        }
    } finally { $zip.Dispose() }
}
