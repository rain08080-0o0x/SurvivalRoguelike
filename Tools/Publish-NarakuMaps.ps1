param(
    [ValidateSet('Publish', 'Restore')][string]$Action = 'Publish'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$editorAssets = Join-Path $repositoryRoot 'NarakuEditor\Assets'
$gameAssets = Join-Path $repositoryRoot 'NarakuGame\Assets'
$gameOutputRoot = Join-Path $repositoryRoot 'Output'
$backupRoot = Join-Path $gameAssets 'Backup\MapPublish'

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    try { $_.Path -like "$gameOutputRoot*\奈落塔.exe" } catch { $false }
}
if ($running) {
    Write-Error '実行中なので保存できません'
}

$lockDirectory = Join-Path $gameAssets 'Temp'
New-Item -ItemType Directory -Path $lockDirectory -Force | Out-Null
$lockPath = Join-Path $lockDirectory 'MapPublish.lock'
try {
    $publishLock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
}
catch {
    Write-Error '実行中なので保存できません'
}

function Assert-ContainedPath {
    param([string]$Path, [string]$Root)
    $resolvedPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $resolvedRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (-not $resolvedPath.StartsWith($resolvedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "対象パスがAssets外です: $resolvedPath"
    }
}

function Copy-DirectoryContent {
    param([string]$Source, [string]$Destination)
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    if (Test-Path -LiteralPath $Source) {
        Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
        }
    }
}

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

if ($Action -eq 'Restore') {
    $latest = Get-ChildItem -LiteralPath $backupRoot -Directory |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $latest) { throw '復元できるマップ発行バックアップがありません。' }
    foreach ($name in @('Maps', 'Models')) {
        $source = Join-Path $latest.FullName $name
        $destination = Join-Path $gameAssets $name
        Assert-ContainedPath -Path $destination -Root $gameAssets
        if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
        Copy-DirectoryContent -Source $source -Destination $destination
    }
    Write-Host "Map publish restored: $($latest.Name)"
    exit 0
}

$sourceMaps = Join-Path $editorAssets 'Maps'
if (-not (Test-Path -LiteralPath $sourceMaps)) { throw "発行元Mapsがありません: $sourceMaps" }
$mapFiles = @(Get-ChildItem -LiteralPath $sourceMaps -Recurse -File -Filter '*.json' |
    Where-Object { $_.Name -notlike 'generation_log*' })
if ($mapFiles.Count -eq 0) { throw '発行対象のMap JSONがありません。' }
foreach ($mapFile in $mapFiles) {
    try {
        Get-Content -LiteralPath $mapFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json | Out-Null
    }
    catch {
        throw "Map JSONの検証に失敗しました: $($mapFile.FullName)"
    }
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backup = Join-Path $backupRoot $stamp
foreach ($name in @('Maps', 'Models')) {
    Copy-DirectoryContent -Source (Join-Path $gameAssets $name) -Destination (Join-Path $backup $name)
}

$stageRoot = Join-Path $gameAssets "Temp\MapPublish_$([guid]::NewGuid().ToString('N'))"
$rollbackRoot = Join-Path $gameAssets "Temp\MapPublishRollback_$([guid]::NewGuid().ToString('N'))"
Assert-ContainedPath -Path $stageRoot -Root $gameAssets
Assert-ContainedPath -Path $rollbackRoot -Root $gameAssets
try {
    Copy-DirectoryContent -Source $sourceMaps -Destination (Join-Path $stageRoot 'Maps')
    Copy-DirectoryContent -Source (Join-Path $editorAssets 'Models') -Destination (Join-Path $stageRoot 'Models')

    New-Item -ItemType Directory -Path $rollbackRoot -Force | Out-Null
    $replacedNames = @()
    try {
        foreach ($name in @('Maps', 'Models')) {
            $destination = Join-Path $gameAssets $name
            Assert-ContainedPath -Path $destination -Root $gameAssets
            if (Test-Path -LiteralPath $destination) {
                Move-Item -LiteralPath $destination -Destination (Join-Path $rollbackRoot $name)
            }
            Move-Item -LiteralPath (Join-Path $stageRoot $name) -Destination $destination
            $replacedNames += $name
        }
    }
    catch {
        foreach ($name in $replacedNames) {
            $destination = Join-Path $gameAssets $name
            if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
        }
        foreach ($name in @('Maps', 'Models')) {
            $previous = Join-Path $rollbackRoot $name
            if (Test-Path -LiteralPath $previous) {
                Move-Item -LiteralPath $previous -Destination (Join-Path $gameAssets $name)
            }
        }
        throw
    }

    [IO.File]::WriteAllText((Join-Path $gameAssets 'Maps\map_version.txt'),
        "$stamp`n$([guid]::NewGuid().ToString('N'))`n", [Text.UTF8Encoding]::new($false))
}
finally {
    if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
    if (Test-Path -LiteralPath $rollbackRoot) { Remove-Item -LiteralPath $rollbackRoot -Recurse -Force }
}

$backups = Get-ChildItem -LiteralPath $backupRoot -Directory | Sort-Object Name -Descending
$backups | Select-Object -Skip 5 | ForEach-Object {
    Assert-ContainedPath -Path $_.FullName -Root $backupRoot
    Remove-Item -LiteralPath $_.FullName -Recurse -Force
}

Write-Host "Map publish succeeded: $stamp"
$publishLock.Dispose()
