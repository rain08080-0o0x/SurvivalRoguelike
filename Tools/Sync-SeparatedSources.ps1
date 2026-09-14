param(
    [switch]$CheckOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$gameSource = Join-Path $repositoryRoot 'NarakuGame\Source'
$editorSource = Join-Path $repositoryRoot 'NarakuEditor\Source'

$commonFiles = @(
    'Defines.h', 'DebugUtil.h', 'DirectX.cpp', 'DirectX.h', 'Easing.h',
    'Geometory.cpp', 'Geometory.h', 'Input.cpp', 'Input.h',
    'InputConfig.cpp', 'InputConfig.h',
    'MeshBuffer.cpp', 'MeshBuffer.h', 'Model.cpp', 'Model.h',
    'NarakuUiNavigation.cpp', 'NarakuUiNavigation.h',
    'Scene.cpp', 'Scene.h', 'SceneNarakuInputSettings.cpp', 'SceneNarakuInputSettings.h',
    'Shader.cpp', 'Shader.h', 'ShaderList.cpp', 'ShaderList.h',
    'Sprite.cpp', 'Sprite.h', 'Texture.cpp', 'Texture.h', 'Transfer.cpp', 'Transfer.h',
    '_geometory.cpp', '_model.cpp',
    'imconfig.h', 'imgui.cpp', 'imgui.h', 'imgui_draw.cpp', 'imgui_impl_dx11.cpp',
    'imgui_impl_dx11.h', 'imgui_impl_win32.cpp', 'imgui_impl_win32.h', 'imgui_internal.h',
    'imgui_tables.cpp', 'imgui_widgets.cpp', 'imstb_rectpack.h', 'imstb_textedit.h',
    'imstb_truetype.h'
)

$gameFiles = @(
    'SceneNarakuProto.cpp', 'SceneNarakuProto.h', 'SceneNarakuResultView.cpp'
)

function Sync-FileSet {
    param(
        [string]$Destination,
        [string[]]$Files
    )

    if (-not $CheckOnly) {
        New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    }

    foreach ($relativePath in $Files) {
        $source = Join-Path $gameSource $relativePath
        $target = Join-Path $Destination $relativePath
        if (-not (Test-Path -LiteralPath $source)) {
            throw "同期元が見つかりません: $source"
        }

        if ($CheckOnly) {
            if (-not (Test-Path -LiteralPath $target)) {
                throw "同期先が見つかりません: $target"
            }
            if ((Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash -ne
                (Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash) {
                throw "同期差分があります: $relativePath"
            }
        }
        else {
            Copy-Item -LiteralPath $source -Destination $target -Force
        }
    }
}

Sync-FileSet -Destination $editorSource -Files ($commonFiles + $gameFiles)

Write-Host ($(if ($CheckOnly) { 'Separated source check succeeded.' } else { 'Separated source synchronization succeeded.' }))
