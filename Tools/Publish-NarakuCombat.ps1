param([string]$EditorAssetsPath = "Assets")

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Split-Path -Parent $scriptRoot
$source = if ([System.IO.Path]::IsPathRooted($EditorAssetsPath)) {
    Join-Path $EditorAssetsPath "Data\Combat"
} else {
    Join-Path (Get-Location) "$EditorAssetsPath\Data\Combat"
}
if (-not (Test-Path -LiteralPath $source)) {
    $source = Join-Path $repositoryRoot "NarakuEditor\Assets\Data\Combat"
}
$destination = Join-Path $repositoryRoot "NarakuGame\Assets\Data\Combat"
if (-not (Test-Path -LiteralPath $source)) { throw "Combat source does not exist: $source" }
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -Path (Join-Path $source "*") -Destination $destination -Force
