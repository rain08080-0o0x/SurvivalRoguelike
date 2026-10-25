param()
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual C++ build tools not found.' }
$scratch = Join-Path $repository ('Temp/CombatTests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$setup = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$test = Join-Path $PSScriptRoot 'Tests/CombatTimelineTests.cpp'
$source = Join-Path $repository 'NarakuGame/Source/NarakuCombatData.cpp'
$exe = Join-Path $scratch 'CombatTimelineTests.exe'
$command = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++14 /utf-8 "{1}" "{2}" /Fe:"{3}"' -f $setup, $test, $source, $exe
Push-Location $scratch
try {
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Combat test compilation failed.' }
    & $exe $scratch (Join-Path $PSScriptRoot 'Tests/Fixtures/LegacyPickaxe.nattack')
    if ($LASTEXITCODE -ne 0) { throw 'Combat timeline tests failed.' }
}
finally { Pop-Location }
