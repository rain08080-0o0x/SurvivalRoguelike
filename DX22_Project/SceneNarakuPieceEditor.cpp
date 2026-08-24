#include "SceneNarakuPieceEditor.h"
#include "EditorPerformanceProfiler.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Model.h"
#include "ShaderList.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX;
#include "NarakuPieceEditorInternal.h"

SceneNarakuPieceEditor::SceneNarakuPieceEditor()
    : m_piece(NarakuPiece::CreateDefaultPiece(NarakuPiece::SizePreset::Size16x16))
    , m_selectedX(0)
    , m_selectedZ(0)
    , m_selectedVertices{ VertexSelection{ 0, 0 } }
    , m_selectedCellX(0)
    , m_selectedCellZ(0)
    , m_selectedCells{ CellSelection{ 0, 0 } }
    , m_saveFileName(L"piece_0001.json")
    , m_validationDirty(true)
{
    EDITOR_PROFILE_FUNCTION();
    SyncSaveFileNameInput();
    ReloadPieceHierarchyEntries();
    LoadEnvironmentModelCatalog();
    UpdateMainWindowTitle();
    ResetCamera();
    UpdateCameraMatrices();
    RefreshValidationIssues();
}

SceneNarakuPieceEditor::~SceneNarakuPieceEditor()
{
    EDITOR_PROFILE_FUNCTION();
    ReleaseEnvironmentModelPopupPreview();
    ReleaseEnvironmentModels();
    ReleasePreviewRenderTarget();
}

void SceneNarakuPieceEditor::Update()
{
    EDITOR_PROFILE_FUNCTION();
    HandleUndoRedoShortcuts();
    ImGuiIO& io = ImGui::GetIO();
    const bool deletePressed = !io.WantTextInput && !io.WantCaptureKeyboard && IsAsyncModifierPressed(VK_DELETE);
    const bool deleteTriggered = IsShortcutTriggered(deletePressed, m_prevDeletePressed);
    UpdateCamera();

    // 現在の編集モードに対応する入力処理だけを実行します。
    if (m_editMode == EditMode::Height)
    {
        UpdateHeightMode(deleteTriggered);
    }
    // 先の条件に該当せず、この条件を満たす場合は、`UpdateGridObjectMode` の処理を実行します。
    else if (m_editMode == EditMode::GridObject)
    {
        UpdateGridObjectMode(deleteTriggered);
    }
    else
    {
        UpdateEnvironmentObjectMode(deleteTriggered);
    }
    UpdateCameraMatrices();

    // 編集で無効化された検証結果だけを必要なフレームに再計算します。
    if (m_validationDirty)
    {
        RefreshValidationIssues();
    }
}

void SceneNarakuPieceEditor::UpdateHeightMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    const bool canDeleteCells =
        m_terrainSelectionMode == TerrainSelectionMode::Cell && !m_selectedCells.empty();
    // セル選択中にDeleteが押された場合だけ選択セルを削除します。
    if (deleteTriggered && canDeleteCells)
    {
        DeleteSelectedCells();
    }
    UpdateHeightEditing();
}

void SceneNarakuPieceEditor::UpdateGridObjectMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    // Delete入力が発生したフレームだけ選択オブジェクトを削除します。
    if (deleteTriggered)
    {
        DeleteSelectedGridObject();
    }
    UpdateGridObjectEditing();
}

void SceneNarakuPieceEditor::UpdateEnvironmentObjectMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    const bool hasSelectedObject =
        m_selectedEnvironmentObjectIndex >= 0 &&
        m_selectedEnvironmentObjectIndex < static_cast<int>(m_piece.environmentObjects.size());
    // 有効な環境オブジェクトを選択している時だけDelete入力を反映します。
    if (deleteTriggered && hasSelectedObject)
    {
        DeleteSelectedEnvironmentObject();
    }
    UpdateEnvironmentObjectEditing();
}

void SceneNarakuPieceEditor::DeleteSelectedCells()
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    // 複数選択中の全セルへ削除状態を反映します。
    for (const CellSelection& selection : m_selectedCells)
    {
        NarakuPiece::CellData* const cell = GetCellData(selection.x, selection.z);
        // 有効範囲内に存在するセルだけを書き換えます。
        if (cell != nullptr)
        {
            cell->deleted = true;
        }
    }
    MarkPieceDirty();
    SetMessage(u8"選択セルを削除しました");
}

void SceneNarakuPieceEditor::DeleteSelectedEnvironmentObject()
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    m_piece.environmentObjects.erase(
        m_piece.environmentObjects.begin() + m_selectedEnvironmentObjectIndex);
    m_selectedEnvironmentObjectIndex = -1;
    MarkPieceDirty();
    SetMessage(u8"環境オブジェクトを削除しました");
}

void SceneNarakuPieceEditor::Draw()
{
    EDITOR_PROFILE_FUNCTION();
    RenderTerrainPreviewToTexture();
    OpenRequestedPopups();
    DrawEditorWindow();
    DrawPreviewWindow();
    DrawHeightGridWindow();
    DrawPieceHierarchyWindow();
    DrawEnvironmentAssetsWindow();
    DrawNewPiecePopup();
    DrawSavePiecePopup();
    DrawRenamePiecePopup();
    DrawEnvironmentModelPopup();
}

void SceneNarakuPieceEditor::OpenRequestedPopups()
{
    EDITOR_PROFILE_FUNCTION();
    // 新規作成要求を一度だけImGuiへ渡します。
    if (m_requestOpenNewPiecePopup)
    {
        ImGui::OpenPopup(u8"新規ピースを作成");
        m_requestOpenNewPiecePopup = false;
    }
    // 保存要求を一度だけImGuiへ渡します。
    if (m_requestOpenSavePiecePopup)
    {
        ImGui::OpenPopup(u8"ピースを保存");
        m_requestOpenSavePiecePopup = false;
    }
    // 名前変更要求を一度だけImGuiへ渡します。
    if (m_requestOpenRenamePiecePopup)
    {
        ImGui::OpenPopup(u8"ピース名を変更");
        m_requestOpenRenamePiecePopup = false;
    }
}

bool SceneNarakuPieceEditor::HandleNativeMenuCommand(unsigned int commandId)
{
    EDITOR_PROFILE_FUNCTION();
    // 値の種類に対応する処理を選択します。
    switch (commandId)
    {
    case MenuNewPiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"新規作成"))
        {
            return true;
        }
        std::fill(m_newPieceFileNameInput.begin(), m_newPieceFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_newPieceFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_newPieceFileNameInput.data());
            m_newPieceFileNameInput[copyLength] = '\0';
        }
        m_newPieceFileName = EnsureJsonFileName(m_saveFileName);
        m_requestOpenNewPiecePopup = true;
        return true;
    case MenuSavePiece:
        SyncSaveFileNameInput();
        m_saveAsDraft = true;
        m_requestOpenSavePiecePopup = true;
        return true;
    case MenuLoadPiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"読込"))
        {
            return true;
        }
        OpenLoadPieceDialog();
        return true;
    case MenuRenamePiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"名前変更"))
        {
            return true;
        }
        std::fill(m_renameFileNameInput.begin(), m_renameFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_renameFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_renameFileNameInput.data());
            m_renameFileNameInput[copyLength] = '\0';
        }
        m_requestOpenRenamePiecePopup = true;
        return true;
    case MenuDeletePiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"削除"))
        {
            return true;
        }
        DeleteCurrentPiece();
        return true;
    case MenuTogglePieceBasicWindow:
        m_showPieceBasicWindow = !m_showPieceBasicWindow;
        return true;
    case MenuTogglePieceConnectionWindow:
        m_showPieceConnectionWindow = !m_showPieceConnectionWindow;
        return true;
    case MenuToggleTerrainEditWindow:
        m_showTerrainEditWindow = !m_showTerrainEditWindow;
        return true;
    case MenuToggleGridObjectPlacementWindow:
        m_showGridObjectPlacementWindow = !m_showGridObjectPlacementWindow;
        return true;
    case MenuToggleGridObjectSelectionWindow:
        m_showGridObjectSelectionWindow = !m_showGridObjectSelectionWindow;
        return true;
    case MenuTogglePieceFileAndValidationWindow:
        m_showPieceFileAndValidationWindow = !m_showPieceFileAndValidationWindow;
        return true;
    case MenuTogglePreviewWindow:
        m_showPreviewWindow = !m_showPreviewWindow;
        return true;
    case MenuToggleHeightGridWindow:
        m_showHeightGridWindow = !m_showHeightGridWindow;
        return true;
    case MenuTogglePieceHierarchyWindow:
        m_showPieceHierarchyWindow = !m_showPieceHierarchyWindow;
        return true;
    case MenuNewEnvironmentModel:
        OpenNewEnvironmentModelDialog();
        return true;
    case MenuDeleteEnvironmentModel:
        DeleteSelectedEnvironmentModel();
        return true;
    case MenuEnvironmentModelSetting:
        OpenEnvironmentModelSetting();
        return true;
    case MenuToggleEnvironmentAssetsWindow:
        m_showEnvironmentAssetsWindow = !m_showEnvironmentAssetsWindow;
        return true;
    default:
        return false;
    }
}

void SceneNarakuPieceEditor::SyncNativeMenuState(HMENU menuBar) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (menuBar == nullptr)
    {
        return;
    }

    SetMenuCheckState(menuBar, MenuTogglePieceBasicWindow, m_showPieceBasicWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceConnectionWindow, m_showPieceConnectionWindow);
    SetMenuCheckState(menuBar, MenuToggleTerrainEditWindow, m_showTerrainEditWindow);
    SetMenuCheckState(menuBar, MenuToggleGridObjectPlacementWindow, m_showGridObjectPlacementWindow);
    SetMenuCheckState(menuBar, MenuToggleGridObjectSelectionWindow, m_showGridObjectSelectionWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceFileAndValidationWindow, m_showPieceFileAndValidationWindow);
    SetMenuCheckState(menuBar, MenuTogglePreviewWindow, m_showPreviewWindow);
    SetMenuCheckState(menuBar, MenuToggleHeightGridWindow, m_showHeightGridWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceHierarchyWindow, m_showPieceHierarchyWindow);
    SetMenuCheckState(menuBar, MenuToggleEnvironmentAssetsWindow, m_showEnvironmentAssetsWindow);
    SetMenuItemLabel(menuBar, MenuFileStatus, BuildEditingStatusLabel());

    // 条件に該当する場合は、`DrawMenuBar` の処理を実行します。
    if (HWND window = GetPreviewHostWindow())
    {
        DrawMenuBar(window);
    }
}


void SceneNarakuPieceEditor::RefreshValidationIssues()
{
    EDITOR_PROFILE_FUNCTION();
    m_validationIssues = NarakuPiece::ValidatePieceData(m_piece);
    // 対象コレクションの各要素を順に処理します。
    for (const NarakuPiece::EnvironmentObjectData& object : m_piece.environmentObjects)
    {
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (FindEnvironmentModelIndexById(object.modelId) < 0)
        {
            NarakuPiece::ValidationIssue issue;
            issue.severity = NarakuPiece::ValidationIssue::Severity::Error;
            issue.message = "environmentObject が未登録モデルを参照しています: " + object.modelId;
            m_validationIssues.push_back(issue);
        }
    }
    m_validationDirty = false;
}

void SceneNarakuPieceEditor::InvalidateValidationState()
{
    EDITOR_PROFILE_FUNCTION();
    m_validationDirty = true;
}

void SceneNarakuPieceEditor::MarkPieceDirty()
{
    EDITOR_PROFILE_FUNCTION();
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = true;
    InvalidateValidationState();
    // 条件に該当する場合は、`UpdateMainWindowTitle` の処理を実行します。
    if (!wasDirty)
    {
        UpdateMainWindowTitle();
    }
}

void SceneNarakuPieceEditor::MarkPieceClean()
{
    EDITOR_PROFILE_FUNCTION();
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = false;
    // 条件に該当する場合は、`UpdateMainWindowTitle` の処理を実行します。
    if (wasDirty)
    {
        UpdateMainWindowTitle();
    }
}

void SceneNarakuPieceEditor::SetMessage(const std::string& message)
{
    EDITOR_PROFILE_FUNCTION();
    m_message = message;
}

std::wstring SceneNarakuPieceEditor::Utf8ToWide(const std::string& text) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (text.empty())
    {
        return std::wstring();
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (length <= 0)
    {
        std::wstring fallback;
        fallback.reserve(text.size());
        // 対象コレクションの各要素を順に処理します。
        for (unsigned char ch : text)
        {
            fallback.push_back(static_cast<wchar_t>(ch));
        }
        return fallback;
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}

std::string SceneNarakuPieceEditor::WideToUtf8(const std::wstring& text) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (text.empty())
    {
        return std::string();
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (length <= 0)
    {
        std::string fallback;
        fallback.reserve(text.size());
        // 対象コレクションの各要素を順に処理します。
        for (wchar_t ch : text)
        {
            fallback.push_back((ch >= 0 && ch <= 0x7f) ? static_cast<char>(ch) : '?');
        }
        return fallback;
    }

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}
