param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'

# Inspect the files we ship, without loading a DLL or changing registration.
# Windows supplies the OS DLLs; an external VC++ runtime must not be required.
function Get-HanBonGoPeImports([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    function Assert-Range([long]$Offset, [long]$Count) {
        if ($Offset -lt 0 -or $Count -lt 0 -or $Offset -gt $bytes.LongLength - $Count) { throw "Invalid PE range in $Path." }
    }
    function Read-U16([long]$Offset) { Assert-Range $Offset 2; [BitConverter]::ToUInt16($bytes, [int]$Offset) }
    function Read-U32([long]$Offset) { Assert-Range $Offset 4; [BitConverter]::ToUInt32($bytes, [int]$Offset) }
    function Read-U64([long]$Offset) { Assert-Range $Offset 8; [BitConverter]::ToUInt64($bytes, [int]$Offset) }
    if ((Read-U16 0) -ne 0x5a4d) { throw "Missing DOS header: $Path." }
    $pe = [long](Read-U32 0x3c)
    if ((Read-U32 $pe) -ne 0x4550) { throw "Missing PE header: $Path." }
    $machine = Read-U16 ($pe + 4)
    $sectionCount = Read-U16 ($pe + 6)
    $optionalSize = Read-U16 ($pe + 20)
    $optional = $pe + 24
    Assert-Range $optional $optionalSize
    $magic = Read-U16 $optional
    if ($magic -eq 0x20b) {
        $directory = $optional + 112; $directoryCount = Read-U32 ($optional + 108)
        $imageBase = Read-U64 ($optional + 24)
    } elseif ($magic -eq 0x10b) {
        $directory = $optional + 96; $directoryCount = Read-U32 ($optional + 92)
        $imageBase = Read-U32 ($optional + 28)
    } else { throw "Unsupported PE optional header: $Path." }
    if ($directory -gt $optional + $optionalSize) { throw "Truncated PE optional header: $Path." }
    $headerSize = Read-U32 ($optional + 60)
    $sections = @()
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $entry = $optional + $optionalSize + $i * 40
        Assert-Range $entry 40
        $sections += [pscustomobject]@{ Rva = [long](Read-U32 ($entry + 12)); Size = [long](Read-U32 ($entry + 16)); Offset = [long](Read-U32 ($entry + 20)) }
    }
    function Resolve-Rva([long]$Rva) {
        if ($Rva -ge 0 -and $Rva -lt $headerSize) { Assert-Range $Rva 1; return $Rva }
        foreach ($section in $sections) {
            if ($Rva -ge $section.Rva -and $Rva -lt $section.Rva + $section.Size) {
                $offset = $section.Offset + $Rva - $section.Rva
                Assert-Range $offset 1
                return $offset
            }
        }
        throw "Unmapped PE address in $Path."
    }
    function Read-ImportName([long]$Rva) {
        $offset = Resolve-Rva $Rva
        $end = $offset
        while ($end -lt $bytes.LongLength -and $bytes[$end] -ne 0 -and $end - $offset -lt 512) { $end++ }
        if ($end -eq $offset -or $end -ge $bytes.LongLength -or $bytes[$end] -ne 0) { throw "Invalid PE import name in $Path." }
        [Text.Encoding]::ASCII.GetString($bytes, [int]$offset, [int]($end - $offset))
    }
    $imports = @()
    # IMAGE_IMPORT_DESCRIPTOR (20 bytes), then delay imports (32 bytes).
    foreach ($table in @(@{ Index = 1; Size = 20; NameOffset = 12 }, @{ Index = 13; Size = 32; NameOffset = 4 })) {
        if ($directoryCount -le $table.Index) { continue }
        $descriptor = $directory + $table.Index * 8
        if ($descriptor + 8 -gt $optional + $optionalSize) { throw "Truncated PE data directory: $Path." }
        $rva = [long](Read-U32 $descriptor); $length = [long](Read-U32 ($descriptor + 4))
        if ($rva -eq 0) { continue }
        $terminated = $false
        for ($position = 0; $position + $table.Size -le $length; $position += $table.Size) {
            $entry = Resolve-Rva ($rva + $position)
            Assert-Range $entry $table.Size
            $nonzero = $false
            for ($field = 0; $field -lt $table.Size; $field += 4) { if ((Read-U32 ($entry + $field)) -ne 0) { $nonzero = $true; break } }
            if (-not $nonzero) { $terminated = $true; break }
            $nameRva = [long](Read-U32 ($entry + $table.NameOffset))
            if ($table.Index -eq 13 -and ((Read-U32 $entry) -band 1) -eq 0) { $nameRva -= $imageBase }
            $imports += Read-ImportName $nameRva
        }
        if (-not $terminated) { throw "Unterminated PE import table: $Path." }
    }
    [pscustomobject]@{ Machine = $machine; Magic = $magic; Imports = @($imports | Sort-Object -Unique) }
}

$checks = 0
foreach ($architecture in @(@{ Name = 'x64'; Machine = 0x8664; Magic = 0x20b }, @{ Name = 'Win32'; Machine = 0x14c; Magic = 0x10b })) {
    foreach ($file in @('HangulJpIME.dll', 'ime-tool.exe')) {
        $path = Join-Path $ProjectRoot ('build\' + $architecture.Name + '\Release\' + $file)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Build $($architecture.Name) Release first: $path." }
        $result = Get-HanBonGoPeImports $path
        if ($result.Machine -ne $architecture.Machine -or $result.Magic -ne $architecture.Magic) { throw "Wrong PE architecture: $path." }
        if ($result.Imports.Count -eq 0) { throw "No PE imports found: $path." }
        $externalRuntime = @($result.Imports | Where-Object { $_ -match '(?i)^(msvcp|vcruntime|concrt|msvcr)\d.*\.dll$' })
        if ($externalRuntime.Count) { throw "External VC++ runtime required by ${path}: $($externalRuntime -join ', ')." }
        $checks++
        Write-Host "$($architecture.Name) $file`: correct PE architecture; no external VC++ runtime."
    }
}
Write-Host "Runtime dependency PASS ($checks native binaries; files inspected only)"
