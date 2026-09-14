Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Sync-SeparatedSources.ps1') -CheckOnly

$editorDirectory = Join-Path $repositoryRoot 'Output\Debug\Editor'
$editorExecutable = Join-Path $editorDirectory 'NarakuEditor.exe'
if (-not (Test-Path -LiteralPath $editorExecutable)) {
    throw "先にNarakuEditor Debug x64をビルドしてください: $editorExecutable"
}

$resultPath = Join-Path $editorDirectory 'Assets\Temp\generation_parity_result.txt'
if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath -Force }
$configPath = Join-Path $editorDirectory 'Assets\Config\naraku_editor_preview.cfg'
$configLines = @('seed=7640891576956012809', 'depth=1', 'sublayer=0', 'area=1', 'spawnAtReturnArea=1')
for ($stage = 0; $stage -lt 15; ++$stage) { $configLines += "areaCount$stage=1" }
[IO.File]::WriteAllLines($configPath, $configLines, [Text.UTF8Encoding]::new($false))

$process = Start-Process -FilePath $editorExecutable -ArgumentList '--verify-generation' `
    -WorkingDirectory $editorDirectory -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(180000)) {
    Stop-Process -Id $process.Id -Force
    throw '同一シード生成検証が180秒以内に完了しませんでした。'
}
if ($process.ExitCode -ne 0) {
    throw "同一シード生成検証に失敗しました。終了コード: $($process.ExitCode)"
}
if (-not (Test-Path -LiteralPath $resultPath) -or
    (Get-Content -LiteralPath $resultPath -Raw).Trim() -ne 'OK') {
    throw '同一シード生成検証の比較結果がOKではありません。'
}

Write-Host 'Generation parity test succeeded (stage counts, connection IDs, piece placement IDs).'
