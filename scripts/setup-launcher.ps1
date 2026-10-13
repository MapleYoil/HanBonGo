param([string]$ReportPath)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
try {
    & (Join-Path $PSScriptRoot 'update.ps1')
    $message = '설치가 완료되었습니다. Win + Space를 눌러 한본어 입력기를 선택하세요.'
    if ($ReportPath) { [IO.File]::WriteAllText($ReportPath, 'PASS') }
    else { [void][Windows.Forms.MessageBox]::Show($message, '한본어 입력기', 'OK', 'Information') }
    exit 0
} catch {
    $message = "설치를 완료하지 못했습니다.`r`n$($_.Exception.Message)"
    if ($ReportPath) { [IO.File]::WriteAllText($ReportPath, $message) }
    else { [void][Windows.Forms.MessageBox]::Show($message, '한본어 입력기', 'OK', 'Error') }
    exit 1
}
