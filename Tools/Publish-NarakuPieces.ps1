param(
    [ValidateSet('Publish', 'Restore')][string]$Action = 'Publish',
    [string]$EditorAssetsPath = 'Assets',
    [string]$GameAssetsPath = '',
    [string]$GameOutputRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$editorAssets = if ([IO.Path]::IsPathRooted($EditorAssetsPath)) {
    [IO.Path]::GetFullPath($EditorAssetsPath)
} else {
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $EditorAssetsPath))
}
$gameAssets = if ($GameAssetsPath) {
    if ([IO.Path]::IsPathRooted($GameAssetsPath)) {
        [IO.Path]::GetFullPath($GameAssetsPath)
    } else {
        [IO.Path]::GetFullPath((Join-Path (Get-Location) $GameAssetsPath))
    }
} else {
    Join-Path $repositoryRoot 'NarakuGame\Assets'
}
$gameOutputRoot = if ($GameOutputRoot) {
    if ([IO.Path]::IsPathRooted($GameOutputRoot)) {
        [IO.Path]::GetFullPath($GameOutputRoot)
    } else {
        [IO.Path]::GetFullPath((Join-Path (Get-Location) $GameOutputRoot))
    }
} else {
    Join-Path $repositoryRoot 'Output'
}
$backupRoot = Join-Path $gameAssets 'Backup\PiecePublish'
$publishLock = $null

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

function Test-FileContentEqual {
    param([string]$First, [string]$Second)
    if (-not (Test-Path -LiteralPath $First) -or -not (Test-Path -LiteralPath $Second)) { return $false }
    if ((Get-Item -LiteralPath $First).Length -ne (Get-Item -LiteralPath $Second).Length) { return $false }
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $firstStream = [IO.File]::OpenRead($First)
        try { $firstHash = [Convert]::ToBase64String($sha256.ComputeHash($firstStream)) }
        finally { $firstStream.Dispose() }
        $secondStream = [IO.File]::OpenRead($Second)
        try { $secondHash = [Convert]::ToBase64String($sha256.ComputeHash($secondStream)) }
        finally { $secondStream.Dispose() }
        return $firstHash -eq $secondHash
    }
    finally { $sha256.Dispose() }
}

function Sync-DirectoryContent {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$ChangedBackup,
        [string]$AddedManifest
    )

    $added = [Collections.Generic.List[string]]::new()
    $copied = 0
    $skipped = 0
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    [IO.File]::WriteAllLines($AddedManifest, $added, [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $Source) {
        Get-ChildItem -LiteralPath $Source -Recurse -File | ForEach-Object {
            $relative = $_.FullName.Substring($Source.Length).TrimStart('\')
            $target = Join-Path $Destination $relative
            Assert-ContainedPath -Path $target -Root $Destination
            if (Test-FileContentEqual -First $_.FullName -Second $target) {
                ++$skipped
                return
            }
            if (Test-Path -LiteralPath $target) {
                $backupPath = Join-Path $ChangedBackup $relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $backupPath) -Force | Out-Null
                Copy-Item -LiteralPath $target -Destination $backupPath -Force
            }
            else {
                $added.Add($relative)
                [IO.File]::WriteAllLines($AddedManifest, $added, [Text.UTF8Encoding]::new($false))
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
            ++$copied
        }
    }
    return [PSCustomObject]@{ Copied = $copied; Skipped = $skipped }
}

function Restore-DirectoryDelta {
    param(
        [string]$Destination,
        [string]$ChangedBackup,
        [string]$AddedManifest
    )

    if (Test-Path -LiteralPath $AddedManifest) {
        Get-Content -LiteralPath $AddedManifest -Encoding UTF8 | Where-Object { $_ } | ForEach-Object {
            $target = Join-Path $Destination $_
            Assert-ContainedPath -Path $target -Root $Destination
            if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
        }
    }
    if (Test-Path -LiteralPath $ChangedBackup) {
        Get-ChildItem -LiteralPath $ChangedBackup -Recurse -File | ForEach-Object {
            $relative = $_.FullName.Substring($ChangedBackup.Length).TrimStart('\')
            $target = Join-Path $Destination $relative
            Assert-ContainedPath -Path $target -Root $Destination
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
        }
    }
}

function Write-NewMapVersion {
    param([string]$AssetsRoot)
    $mapsDirectory = Join-Path $AssetsRoot 'Maps'
    New-Item -ItemType Directory -Path $mapsDirectory -Force | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    [IO.File]::WriteAllText(
        (Join-Path $mapsDirectory 'map_version.txt'),
        "$stamp`n$([guid]::NewGuid().ToString('N'))`n",
        [Text.UTF8Encoding]::new($false))
}

function Install-PublishedData {
    param([string]$StageRoot, [string]$RollbackRoot, [string]$AssetsRoot)

    New-Item -ItemType Directory -Path $RollbackRoot -Force | Out-Null
    $destinations = @(
        @{ Name = 'Pieces'; Path = (Join-Path $AssetsRoot 'Naraku\Pieces') }
    )
    $destinations += @{
        Name = 'environment_models.cfg'
        Path = (Join-Path $AssetsRoot 'Naraku\environment_models.cfg')
    }
    $mapVersion = Join-Path $AssetsRoot 'Maps\map_version.txt'
    if (Test-Path -LiteralPath $mapVersion) {
        Copy-Item -LiteralPath $mapVersion -Destination (Join-Path $RollbackRoot 'map_version.txt') -Force
    }

    $touched = @()
    try {
        foreach ($item in $destinations) {
            Assert-ContainedPath -Path $item.Path -Root $AssetsRoot
            if (Test-Path -LiteralPath $item.Path) {
                Move-Item -LiteralPath $item.Path -Destination (Join-Path $RollbackRoot $item.Name)
            }
            $touched += $item
            New-Item -ItemType Directory -Path (Split-Path -Parent $item.Path) -Force | Out-Null
            Move-Item -LiteralPath (Join-Path $StageRoot $item.Name) -Destination $item.Path
        }
        Write-NewMapVersion -AssetsRoot $AssetsRoot
    }
    catch {
        foreach ($item in $touched) {
            if (Test-Path -LiteralPath $item.Path) {
                Remove-Item -LiteralPath $item.Path -Recurse -Force
            }
        }
        foreach ($item in $destinations) {
            $previous = Join-Path $RollbackRoot $item.Name
            if (Test-Path -LiteralPath $previous) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $item.Path) -Force | Out-Null
                Move-Item -LiteralPath $previous -Destination $item.Path
            }
        }
        if (Test-Path -LiteralPath $mapVersion) {
            Remove-Item -LiteralPath $mapVersion -Force
        }
        $previousMapVersion = Join-Path $RollbackRoot 'map_version.txt'
        if (Test-Path -LiteralPath $previousMapVersion) {
            Copy-Item -LiteralPath $previousMapVersion -Destination $mapVersion -Force
        }
        throw
    }
}

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    try { $_.Path -like "$gameOutputRoot*\奈落塔.exe" } catch { $false }
}
if ($running) {
    [Console]::Error.WriteLine('実行中なので保存できません')
    exit 2
}

$lockDirectory = Join-Path $gameAssets 'Temp'
New-Item -ItemType Directory -Path $lockDirectory -Force | Out-Null
$lockPath = Join-Path $lockDirectory 'PiecePublish.lock'
try {
    try {
        $publishLock = [IO.File]::Open(
            $lockPath,
            [IO.FileMode]::OpenOrCreate,
            [IO.FileAccess]::ReadWrite,
            [IO.FileShare]::None)
    }
    catch {
        [Console]::Error.WriteLine('実行中なので保存できません')
        exit 2
    }

    New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

    if ($Action -eq 'Restore') {
        $latest = Get-ChildItem -LiteralPath $backupRoot -Directory |
            Sort-Object Name -Descending | Select-Object -First 1
        if (-not $latest) {
            throw '復元できる小ステージ発行バックアップがありません。'
        }

        foreach ($name in @('Pieces', 'environment_models.cfg')) {
            if (-not (Test-Path -LiteralPath (Join-Path $latest.FullName $name))) {
                throw "復元元バックアップが不完全です: $name"
            }
        }

        $restoreStageRoot = Join-Path $gameAssets "Temp\PieceRestore_$([guid]::NewGuid().ToString('N'))"
        $restoreRollbackRoot = Join-Path $gameAssets "Temp\PieceRestoreRollback_$([guid]::NewGuid().ToString('N'))"
        Assert-ContainedPath -Path $restoreStageRoot -Root $gameAssets
        Assert-ContainedPath -Path $restoreRollbackRoot -Root $gameAssets
        try {
            Copy-DirectoryContent -Source (Join-Path $latest.FullName 'Pieces') -Destination (Join-Path $restoreStageRoot 'Pieces')
            Copy-Item -LiteralPath (Join-Path $latest.FullName 'environment_models.cfg') -Destination (Join-Path $restoreStageRoot 'environment_models.cfg') -Force
            Install-PublishedData -StageRoot $restoreStageRoot -RollbackRoot $restoreRollbackRoot -AssetsRoot $gameAssets

            if (Test-Path -LiteralPath (Join-Path $latest.FullName 'publish_delta_v2.txt')) {
                Restore-DirectoryDelta `
                    -Destination (Join-Path $gameAssets 'Models') `
                    -ChangedBackup (Join-Path $latest.FullName 'ModelsChanged') `
                    -AddedManifest (Join-Path $latest.FullName 'ModelsAdded.txt')
                Restore-DirectoryDelta `
                    -Destination (Join-Path $gameAssets 'Model') `
                    -ChangedBackup (Join-Path $latest.FullName 'ModelChanged') `
                    -AddedManifest (Join-Path $latest.FullName 'ModelAdded.txt')
            }
            else {
                foreach ($name in @('Models', 'Model')) {
                    $snapshot = Join-Path $latest.FullName $name
                    if (-not (Test-Path -LiteralPath $snapshot)) { continue }
                    $destination = Join-Path $gameAssets $name
                    Assert-ContainedPath -Path $destination -Root $gameAssets
                    if (Test-Path -LiteralPath $destination) {
                        Remove-Item -LiteralPath $destination -Recurse -Force
                    }
                    Copy-DirectoryContent -Source $snapshot -Destination $destination
                }
            }
        }
        finally {
            if (Test-Path -LiteralPath $restoreStageRoot) {
                Remove-Item -LiteralPath $restoreStageRoot -Recurse -Force
            }
            if (Test-Path -LiteralPath $restoreRollbackRoot) {
                Remove-Item -LiteralPath $restoreRollbackRoot -Recurse -Force
            }
        }
        Write-Host "Piece publish restored: $($latest.Name)"
        exit 0
    }

    $sourcePieces = Join-Path $editorAssets 'Naraku\Pieces'
    $sourceModels = Join-Path $editorAssets 'Models'
    $sourceLegacyModels = Join-Path $editorAssets 'Model'
    $sourceCatalog = Join-Path $editorAssets 'Naraku\environment_models.cfg'
    if (-not (Test-Path -LiteralPath $sourcePieces)) {
        throw "発行元Piecesがありません: $sourcePieces"
    }
    if (-not (Test-Path -LiteralPath $sourceModels)) {
        throw "発行元Modelsがありません: $sourceModels"
    }
    if (-not (Test-Path -LiteralPath $sourceCatalog)) {
        throw "発行元モデル登録簿がありません: $sourceCatalog"
    }

    $pieceFiles = @(Get-ChildItem -LiteralPath $sourcePieces -Recurse -File -Filter '*.json')
    if ($pieceFiles.Count -eq 0) {
        throw '発行対象の小ステージJSONがありません。'
    }
    $referencedModelIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($pieceFile in $pieceFiles) {
        try {
            $pieceData = Get-Content -LiteralPath $pieceFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
            $environmentObjectsProperty = $pieceData.PSObject.Properties['environmentObjects']
            if ($environmentObjectsProperty) {
                foreach ($environmentObject in @($environmentObjectsProperty.Value)) {
                    $modelIdProperty = $environmentObject.PSObject.Properties['modelId']
                    if ($modelIdProperty -and -not [string]::IsNullOrWhiteSpace([string]$modelIdProperty.Value)) {
                        $referencedModelIds.Add([string]$modelIdProperty.Value) | Out-Null
                    }
                }
            }
        }
        catch {
            throw "小ステージJSONの検証に失敗しました: $($pieceFile.FullName)"
        }
    }

    $registeredModelIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($catalogLine in Get-Content -LiteralPath $sourceCatalog -Encoding UTF8) {
        if ($catalogLine -match '^\s*"([^"]+)"') {
            $registeredModelIds.Add($Matches[1]) | Out-Null
        }
    }
    $unregisteredModelIds = @($referencedModelIds | Where-Object { -not $registeredModelIds.Contains($_) } | Sort-Object)
    if ($unregisteredModelIds.Count -gt 0) {
        Write-Warning "モデル登録簿に存在しない環境モデルIDがあります。該当オブジェクトはGameで描画されません: $($unregisteredModelIds -join ', ')"
    }

    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
    $backup = Join-Path $backupRoot $stamp
    Copy-DirectoryContent -Source (Join-Path $gameAssets 'Naraku\Pieces') -Destination (Join-Path $backup 'Pieces')
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $backup 'publish_delta_v2.txt'), "2`n", [Text.UTF8Encoding]::new($false))
    $gameCatalog = Join-Path $gameAssets 'Naraku\environment_models.cfg'
    if (Test-Path -LiteralPath $gameCatalog) {
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        Copy-Item -LiteralPath $gameCatalog -Destination (Join-Path $backup 'environment_models.cfg') -Force
    }

    $stageRoot = Join-Path $gameAssets "Temp\PiecePublish_$([guid]::NewGuid().ToString('N'))"
    $rollbackRoot = Join-Path $gameAssets "Temp\PiecePublishRollback_$([guid]::NewGuid().ToString('N'))"
    Assert-ContainedPath -Path $stageRoot -Root $gameAssets
    Assert-ContainedPath -Path $rollbackRoot -Root $gameAssets
    $modelSyncStarted = $false
    try {
        $stagedPieces = Join-Path $stageRoot 'Pieces'
        New-Item -ItemType Directory -Path $stagedPieces -Force | Out-Null
        foreach ($pieceFile in $pieceFiles) {
            $relativePath = $pieceFile.FullName.Substring($sourcePieces.Length).TrimStart('\')
            $destination = Join-Path $stagedPieces $relativePath
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $pieceFile.FullName -Destination $destination -Force
        }
        Copy-Item -LiteralPath $sourceCatalog -Destination (Join-Path $stageRoot 'environment_models.cfg') -Force

        $modelSyncStarted = $true
        $modelsResult = Sync-DirectoryContent `
            -Source $sourceModels `
            -Destination (Join-Path $gameAssets 'Models') `
            -ChangedBackup (Join-Path $backup 'ModelsChanged') `
            -AddedManifest (Join-Path $backup 'ModelsAdded.txt')
        $legacyModelsResult = [PSCustomObject]@{ Copied = 0; Skipped = 0 }
        if (Test-Path -LiteralPath $sourceLegacyModels) {
            $legacyModelsResult = Sync-DirectoryContent `
                -Source $sourceLegacyModels `
                -Destination (Join-Path $gameAssets 'Model') `
                -ChangedBackup (Join-Path $backup 'ModelChanged') `
                -AddedManifest (Join-Path $backup 'ModelAdded.txt')
        }

        Install-PublishedData -StageRoot $stageRoot -RollbackRoot $rollbackRoot -AssetsRoot $gameAssets
    }
    catch {
        if ($modelSyncStarted) {
            Restore-DirectoryDelta `
                -Destination (Join-Path $gameAssets 'Models') `
                -ChangedBackup (Join-Path $backup 'ModelsChanged') `
                -AddedManifest (Join-Path $backup 'ModelsAdded.txt')
            Restore-DirectoryDelta `
                -Destination (Join-Path $gameAssets 'Model') `
                -ChangedBackup (Join-Path $backup 'ModelChanged') `
                -AddedManifest (Join-Path $backup 'ModelAdded.txt')
        }
        throw
    }
    finally {
        if (Test-Path -LiteralPath $stageRoot) {
            Remove-Item -LiteralPath $stageRoot -Recurse -Force
        }
        if (Test-Path -LiteralPath $rollbackRoot) {
            Remove-Item -LiteralPath $rollbackRoot -Recurse -Force
        }
    }

    $backups = Get-ChildItem -LiteralPath $backupRoot -Directory | Sort-Object Name -Descending
    $backups | Select-Object -Skip 5 | ForEach-Object {
        Assert-ContainedPath -Path $_.FullName -Root $backupRoot
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }

    $copiedModelCount = $modelsResult.Copied + $legacyModelsResult.Copied
    $skippedModelCount = $modelsResult.Skipped + $legacyModelsResult.Skipped
    Write-Host "Piece publish succeeded: $stamp (models copied: $copiedModelCount, unchanged skipped: $skippedModelCount)"
}
finally {
    if ($publishLock) {
        $publishLock.Dispose()
    }
}
