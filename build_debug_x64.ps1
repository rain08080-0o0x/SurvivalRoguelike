Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectPath = Join-Path $repoRoot "DX22_Project\DX22_Project.vcxproj"

if (-not (Test-Path -LiteralPath $projectPath)) {
    Write-Error "Project file not found: $projectPath"
}

function Find-MSBuildPath {
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
        if ($found) {
            return ($found | Select-Object -First 1)
        }
    }

    return $null
}

$msbuildPath = Find-MSBuildPath
if (-not $msbuildPath) {
    Write-Error "MSBuild not found. Run from Developer PowerShell for Visual Studio or install Build Tools."
}

Write-Host "MSBuild: $msbuildPath"
Write-Host "Project: $projectPath"

& $msbuildPath $projectPath /p:Configuration=Debug /p:Platform=x64 /nologo
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Error "Build failed with exit code: $exitCode"
}

Write-Host "Build succeeded (Debug|x64)."
