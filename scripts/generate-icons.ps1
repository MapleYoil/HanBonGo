$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot = Split-Path -Parent $PSScriptRoot
$resourceRoot = Join-Path $projectRoot 'src\resources'
New-Item -ItemType Directory -Force -Path $resourceRoot | Out-Null
$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
$images = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $bitmap = New-Object Drawing.Bitmap($size, $size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $font = New-Object Drawing.Font('Segoe UI', ([single]($size * 0.46)), [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
    $background = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(35, 62, 100))
    $format = New-Object Drawing.StringFormat
    $stream = New-Object IO.MemoryStream
    try {
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $graphics.FillRectangle($background, 0, 0, $size, $size)
        $format.Alignment = [Drawing.StringAlignment]::Center
        $format.LineAlignment = [Drawing.StringAlignment]::Center
        $graphics.DrawString('HB', $font, [Drawing.Brushes]::White, (New-Object Drawing.RectangleF(0, 0, $size, $size)), $format)
        $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
        $images.Add($stream.ToArray())
        if ($size -eq 32) { $bitmap.Save((Join-Path $resourceRoot 'HanBonGo-preview.png'), [Drawing.Imaging.ImageFormat]::Png) }
    } finally { $stream.Dispose(); $format.Dispose(); $background.Dispose(); $font.Dispose(); $graphics.Dispose(); $bitmap.Dispose() }
}
$output = New-Object IO.MemoryStream
$writer = New-Object IO.BinaryWriter($output)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($bytes in $images) { $writer.Write($bytes) }
    $writer.Flush()
    $iconPath = Join-Path $resourceRoot 'HanBonGo.ico'
    $iconBytes = $output.ToArray()
    if (-not (Test-Path -LiteralPath $iconPath) -or [Convert]::ToBase64String([IO.File]::ReadAllBytes($iconPath)) -ne [Convert]::ToBase64String($iconBytes)) { [IO.File]::WriteAllBytes($iconPath, $iconBytes) }
} finally { $writer.Dispose(); $output.Dispose() }
