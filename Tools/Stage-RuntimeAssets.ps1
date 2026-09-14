param(
    [Parameter(Mandatory = $true)][string]$Role,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][string]$ProjectDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$ProjectDirectory = [IO.Path]::GetFullPath($ProjectDirectory).TrimEnd('\')
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$sourceAssets = Join-Path $repositoryRoot 'Assets'
$legacyAssets = Join-Path $repositoryRoot 'DX22_Project\Assets'
$projectAssets = Join-Path $ProjectDirectory 'Assets'
$outputAssets = Join-Path $OutputDirectory 'Assets'

function Reset-AssetDirectory {
    param([string]$Path, [string]$Root)
    $resolvedPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $resolvedRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (-not $resolvedPath.StartsWith($resolvedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "削除対象が出力Assets外です: $resolvedPath"
    }
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
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

function Copy-AssetTree {
    param(
        [string]$Source,
        [string]$Destination,
        [switch]$PreservePublishedMaps,
        [switch]$PreservePublishedPieces,
        [switch]$PreserveUserSettings,
        [switch]$PreserveEditorRegistries,
        [switch]$FillMissingOnly
    )
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($Source.Length).TrimStart('\')
        $parts = $relative -split '\\'
        if ($parts[0] -in @('Save', 'Logs', 'Temp', 'Tmp', 'Backup')) { return }
        if ($PreserveUserSettings -and $parts[0] -eq 'Config' -and $parts.Count -gt 1 -and
            $parts[1] -eq 'User' -and (Test-Path -LiteralPath (Join-Path $Destination $relative))) { return }
        if ($PreserveEditorRegistries -and
            $relative -in @('Naraku\environment_models.cfg', 'Naraku\Pieces\piece_hierarchy.cfg') -and
            (Test-Path -LiteralPath (Join-Path $Destination $relative))) { return }
        if ($PreservePublishedMaps -and $parts[0] -eq 'Maps' -and
            (Test-Path -LiteralPath (Join-Path $Destination $relative))) { return }
        if ($PreservePublishedPieces -and $parts.Count -gt 1 -and
            $parts[0] -eq 'Naraku' -and $parts[1] -eq 'Pieces' -and
            (Test-Path -LiteralPath (Join-Path $Destination 'Naraku\Pieces'))) { return }
        $target = Join-Path $Destination $relative
        if ($FillMissingOnly -and (Test-Path -LiteralPath $target)) { return }
        if (Test-FileContentEqual -First $_.FullName -Second $target) { return }
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $target -Force
    }
}

if ($Role -eq 'Legacy') {
    Copy-AssetTree -Source $legacyAssets -Destination $outputAssets -PreserveUserSettings
}
else {
    $preservePublishedPieces = $Role -in @('Editor', 'Game')
    $preserveProjectRegistries = $Role -in @('Editor', 'Game')
    $preserveOutputRegistries = $Role -eq 'Editor'
    Copy-AssetTree -Source $sourceAssets -Destination $projectAssets -PreservePublishedMaps -PreserveUserSettings `
        -PreservePublishedPieces:$preservePublishedPieces -PreserveEditorRegistries:$preserveProjectRegistries
    Copy-AssetTree -Source $legacyAssets -Destination $projectAssets -FillMissingOnly
    if ($Role -eq 'Game' -and (Test-Path -LiteralPath (Join-Path $projectAssets 'Naraku\Pieces'))) {
        Reset-AssetDirectory -Path (Join-Path $outputAssets 'Naraku\Pieces') -Root $outputAssets
    }
    Copy-AssetTree -Source $projectAssets -Destination $outputAssets -PreserveUserSettings `
        -PreserveEditorRegistries:$preserveOutputRegistries
}

$dllSuffix = if ($Configuration -eq 'Debug') { 'mtd' } else { 'mt' }
$assimpDll = Join-Path $repositoryRoot "DX22_Project\assimp-vc143-$dllSuffix.dll"
$targetDll = Join-Path $OutputDirectory (Split-Path -Leaf $assimpDll)
if (-not (Test-Path -LiteralPath $targetDll) -or ((Get-Item -LiteralPath $assimpDll).Length -ne (Get-Item -LiteralPath $targetDll).Length)) {
    Copy-Item -LiteralPath $assimpDll -Destination $targetDll -Force
}

Write-Host "Assets staged for $Role ($Configuration)."
