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
#include <utility>

using namespace DirectX;
#include "NarakuPieceEditorInternal.h"

void SceneNarakuPieceEditor::SyncSaveFileNameInput()
{
    EDITOR_PROFILE_FUNCTION();
    m_saveFileName = EnsureJsonFileName(m_saveFileName);
    std::fill(m_saveFileNameInput.begin(), m_saveFileNameInput.end(), '\0');

    const std::string fileNameUtf8 = WideToUtf8(m_saveFileName);
    const size_t copyLength = std::min(fileNameUtf8.size(), m_saveFileNameInput.size() - 1);
    std::copy_n(fileNameUtf8.data(), copyLength, m_saveFileNameInput.data());
    m_saveFileNameInput[copyLength] = '\0';
    UpdateMainWindowTitle();
}

void SceneNarakuPieceEditor::CommitSaveFileNameInput()
{
    EDITOR_PROFILE_FUNCTION();
    m_saveFileName = EnsureJsonFileName(Utf8ToWide(m_saveFileNameInput.data()));
    UpdateMainWindowTitle();
}

void SceneNarakuPieceEditor::CommitNewPieceFileNameInput()
{
    EDITOR_PROFILE_FUNCTION();
    m_newPieceFileName = EnsureJsonFileName(Utf8ToWide(m_newPieceFileNameInput.data()));
}

std::wstring SceneNarakuPieceEditor::GetDisplayFileName() const
{
    EDITOR_PROFILE_FUNCTION();
    return m_saveFileName.empty() ? L"(unnamed)" : m_saveFileName;
}

std::wstring SceneNarakuPieceEditor::BuildEditingStatusLabel() const
{
    EDITOR_PROFILE_FUNCTION();
    std::wstring label = L"\x7DE8\x96C6\x4E2D: ";
    label += GetDisplayFileName();
    // 条件に該当する場合は、`label` の状態を更新します。
    if (m_isPieceDirty)
    {
        label += L" *";
    }
    label += m_isPieceDirty ? L" [\x672A\x4FDD\x5B58]" : L" [\x4FDD\x5B58\x6E08\x307F]";
    label += m_saveAsDraft ? L" [\x4E0B\x66F8\x304D]" : L" [\x5B8C\x6210]";
    return label;
}

void SceneNarakuPieceEditor::UpdateMainWindowTitle() const
{
    EDITOR_PROFILE_FUNCTION();
    HWND mainWindow = ::GetActiveWindow();
    // 条件に該当する場合は、`mainWindow` の状態を更新します。
    if (mainWindow == nullptr)
    {
        mainWindow = ::GetForegroundWindow();
    }
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (mainWindow == nullptr)
    {
        return;
    }

    std::wstring title = L"NarakuProto - PieceEditor - ";
    title += GetDisplayFileName();
    // 条件に該当する場合は、`title` の状態を更新します。
    if (m_isPieceDirty)
    {
        title += L" *";
    }
    title += m_saveAsDraft ? L" [Draft]" : L" [Completed]";
    ::SetWindowTextW(mainWindow, title.c_str());
    // 条件に該当する場合は、`SyncNativeMenuState` の処理を実行します。
    if (HMENU menuBar = ::GetMenu(mainWindow))
    {
        SyncNativeMenuState(menuBar);
    }
}

std::wstring SceneNarakuPieceEditor::GetCurrentSaveTargetPath() const
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring fileName = EnsureJsonFileName(m_saveFileName);
    return m_saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, fileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, fileName);
}

bool SceneNarakuPieceEditor::SavePiece(bool saveAsDraft)
{
    EDITOR_PROFILE_FUNCTION();
    CommitSaveFileNameInput();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (m_saveFileName.empty())
    {
        SetMessage(u8"保存ファイル名を入力してください");
        return false;
    }
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (HasInvalidFileNameChar(m_saveFileName))
    {
        SetMessage(u8"保存ファイル名に使用できない文字が含まれています");
        return false;
    }

    // 条件に該当する場合は、`RefreshValidationIssues` の処理を実行します。
    if (!saveAsDraft)
    {
        RefreshValidationIssues();
        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (NarakuPiece::HasValidationError(m_validationIssues))
        {
            SetMessage(u8"検証エラーがあるため、完成保存を中止しました");
            return false;
        }
    }

    m_piece.lastModified = WideToUtf8(BuildCurrentDateTimeString());
    std::string error;
    const std::wstring savePath = saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, m_saveFileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, m_saveFileName);
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!NarakuPiece::SavePieceData(m_piece, savePath, &error))
    {
        SetMessage(std::string(saveAsDraft ? u8"下書き保存失敗: " : u8"完成保存失敗: ") + error);
        return false;
    }

    SetMessage(saveAsDraft ? u8"下書き保存に成功しました" : u8"完成保存に成功しました");
    m_saveAsDraft = saveAsDraft;
    RegisterPieceHierarchyEntry(savePath, !saveAsDraft, Utf8ToWide(m_piece.lastModified));
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"ピース保存には成功しましたがHierarchy登録ファイルの保存に失敗しました");
    }
    SyncSaveFileNameInput();
    MarkPieceClean();
    return true;
}

bool SceneNarakuPieceEditor::ConfirmDiscardDirtyChanges(const wchar_t* actionName)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_isPieceDirty)
    {
        return true;
    }

    std::wstring message = L"現在のピースは未保存です。";
    // 条件に該当する場合は、`message` の状態を更新します。
    if (actionName != nullptr && actionName[0] != L'\0')
    {
        message += actionName;
        message += L"の前に保存しますか?";
    }
    else
    {
        message += L"保存しますか?";
    }

    const int result = ::MessageBoxW(
        ::GetActiveWindow(),
        message.c_str(),
        L"PieceEditor",
        MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (result == IDYES)
    {
        return SavePiece(m_saveAsDraft);
    }
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (result == IDNO)
    {
        return true;
    }
    return false;
}

bool SceneNarakuPieceEditor::LoadPieceFromPath(const std::wstring& path)
{
    EDITOR_PROFILE_FUNCTION();
    NarakuPiece::PieceData loadedPiece;
    std::string error;
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!NarakuPiece::LoadPieceData(path, loadedPiece, &error))
    {
        SetMessage(std::string(u8"下書き読込失敗: ") + error);
        return false;
    }

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (loadedPiece.sizePreset != NarakuPiece::SizePreset::Size16x16 ||
        loadedPiece.gridWidth != 16 ||
        loadedPiece.gridDepth != 16)
    {
        SetMessage(u8"16x16 以外のピースはこのエディターでは読込できません");
        return false;
    }

    ApplyLoadedPiece(loadedPiece);
    m_saveFileName = EnsureJsonFileName(GetFileNamePart(path));
    m_saveAsDraft = !IsCompletedPiecePath(path);
    SyncSaveFileNameInput();
    MarkPieceClean();
    RegisterPieceHierarchyEntry(path, IsCompletedPiecePath(path), Utf8ToWide(m_piece.lastModified));
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"ピース読込には成功しましたがHierarchy登録ファイルの保存に失敗しました");
        return true;
    }
    SetMessage(u8"下書き読込に成功しました");
    return true;
}

void SceneNarakuPieceEditor::ApplyLoadedPiece(const NarakuPiece::PieceData& loadedPiece)
{
    EDITOR_PROFILE_FUNCTION();
    m_piece = loadedPiece;
    ClearTerrainSelection();
    m_selectedX = ClampInt(0, 0, m_piece.gridWidth - 1);
    m_selectedZ = ClampInt(0, 0, m_piece.gridDepth - 1);
    m_selectedCellX = ClampInt(0, 0, std::max(0, m_piece.gridWidth - 2));
    m_selectedCellZ = ClampInt(0, 0, std::max(0, m_piece.gridDepth - 2));
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    ClearGridObjectSelection();
    m_selectedEnvironmentObjectIndex = -1;
    m_history.Clear();
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
    InvalidateValidationState();
    RefreshValidationIssues();
}

void SceneNarakuPieceEditor::CreateNewPiece(const std::wstring& fileName)
{
    EDITOR_PROFILE_FUNCTION();
    m_piece = NarakuPiece::CreateDefaultPiece(NarakuPiece::SizePreset::Size16x16);
    // 対象コレクションの各要素を順に処理します。
    for (NarakuPiece::CellData& cell : m_piece.cells)
    {
        cell.deleted = false;
    }

    m_selectedX = 0;
    m_selectedZ = 0;
    m_selectedVertices.clear();
    m_selectedVertices.push_back(VertexSelection{ 0, 0 });
    m_selectedCellX = 0;
    m_selectedCellZ = 0;
    m_selectedCells.clear();
    m_selectedCells.push_back(CellSelection{ 0, 0 });
    m_editMode = EditMode::Height;
    m_terrainSelectionMode = TerrainSelectionMode::Vertex;
    m_gridObjectTool = GridObjectTool::MiningPoint;
    m_selectedGridObjectKind = GridObjectKind::None;
    m_selectedMiningPointIndex = -1;
    m_selectedEnvironmentObjectIndex = -1;
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
    m_newMiningVisualType = 0;
    m_newMiningInitiallyRecorded = false;
    m_saveFileName = EnsureJsonFileName(fileName);
    m_saveAsDraft = true;
    m_history.Clear();
    m_validationIssues.clear();
    m_message.clear();
    m_heightDragFloatEditing = false;
    m_draggingHeight = false;
    SyncSaveFileNameInput();
    InvalidateValidationState();
    RefreshValidationIssues();
    MarkPieceDirty();
}

void SceneNarakuPieceEditor::OpenLoadPieceDialog()
{
    EDITOR_PROFILE_FUNCTION();
    wchar_t filePath[MAX_PATH] = {};
    const std::wstring initialPath = NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, L"sample.json");
    const std::wstring initialDirectory = GetDirectoryPart(initialPath);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ::GetActiveWindow();
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = static_cast<DWORD>(std::size(filePath));
    ofn.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!::GetOpenFileNameW(&ofn))
    {
        return;
    }

    LoadPieceFromPath(filePath);
}

bool SceneNarakuPieceEditor::RenameCurrentPiece()
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring newFileName = EnsureJsonFileName(Utf8ToWide(m_renameFileNameInput.data()));
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (newFileName.empty() || HasInvalidFileNameChar(newFileName))
    {
        SetMessage(u8"変更後ファイル名に使用できない文字が含まれています");
        return false;
    }

    const std::wstring oldPath = GetCurrentSaveTargetPath();
    const std::wstring oldRelativePath = NormalizePieceHierarchyPath(oldPath);
    const std::wstring newPath = m_saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, newFileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, newFileName);
    const std::wstring newRelativePath = NormalizePieceHierarchyPath(newPath);

    // 条件に該当する場合は、`ToLowerWide` の処理を実行します。
    if (ToLowerWide(NormalizePathSeparators(oldRelativePath)) ==
        ToLowerWide(NormalizePathSeparators(newRelativePath)))
    {
        m_saveFileName = newFileName;
        SyncSaveFileNameInput();
        SetMessage(u8"ピース名を更新しました");
        return true;
    }

    const std::wstring oldAbsolutePath = ResolvePieceHierarchyPath(oldRelativePath);
    const std::wstring newAbsolutePath = ResolvePieceHierarchyPath(newRelativePath);
    const bool hadExistingFile = PathExists(oldAbsolutePath);
    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (hadExistingFile)
    {
        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (PathExists(newAbsolutePath))
        {
            SetMessage(u8"変更先のファイル名は既に存在します");
            return false;
        }

        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (!::MoveFileExW(oldAbsolutePath.c_str(), newAbsolutePath.c_str(), MOVEFILE_COPY_ALLOWED))
        {
            SetMessage(u8"ファイル名の変更に失敗しました");
            return false;
        }
    }

    const std::wstring lastModified = hadExistingFile
        ? BuildCurrentDateTimeString()
        : Utf8ToWide(m_piece.lastModified);
    RemovePieceHierarchyEntry(oldRelativePath);
    RegisterPieceHierarchyEntry(newRelativePath, !m_saveAsDraft, lastModified);
    SavePieceHierarchyEntries();

    m_saveFileName = newFileName;
    // 条件に該当する場合は、`m_piece.lastModified` の状態を更新します。
    if (!lastModified.empty())
    {
        m_piece.lastModified = WideToUtf8(lastModified);
    }
    SyncSaveFileNameInput();
    // 条件に該当する場合は、`MarkPieceClean` の処理を実行します。
    if (hadExistingFile)
    {
        MarkPieceClean();
    }
    else
    {
        MarkPieceDirty();
    }

    SetMessage(u8"ピース名を変更しました");
    return true;
}

bool SceneNarakuPieceEditor::DeleteCurrentPiece()
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring targetPath = GetCurrentSaveTargetPath();
    const std::wstring targetRelativePath = NormalizePieceHierarchyPath(targetPath);
    const std::wstring targetAbsolutePath = ResolvePieceHierarchyPath(targetRelativePath);
    const std::wstring displayName = GetDisplayFileName();

    std::wstring confirmMessage = L"";
    confirmMessage += displayName;
    confirmMessage += L" を削除しますか?";
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (::MessageBoxW(::GetActiveWindow(), confirmMessage.c_str(), L"PieceEditor", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
    {
        return false;
    }

    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (PathExists(targetAbsolutePath) && !::DeleteFileW(targetAbsolutePath.c_str()))
    {
        SetMessage(u8"ファイル削除に失敗しました");
        return false;
    }

    RemovePieceHierarchyEntry(targetRelativePath);
    SavePieceHierarchyEntries();
    CreateNewPiece(displayName);
    SetMessage(u8"ピースを削除して新規状態へ移行しました");
    return true;
}

void SceneNarakuPieceEditor::DrawPieceHierarchyWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showPieceHierarchyWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"小ステージHierarchy");
    ImGui::SetNextWindowPos(ImVec2(1204.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 300.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`ImGui::End` の処理を実行します。
    if (!ImGui::Begin(u8"小ステージHierarchy", &m_showPieceHierarchyWindow))
    {
        ImGui::End();
        return;
    }

    // 条件に該当する場合は、`ReloadPieceHierarchyEntries` の処理を実行します。
    if (ImGui::Button(u8"再読込"))
    {
        ReloadPieceHierarchyEntries();
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`RegisterCurrentPieceToHierarchy` の処理を実行します。
    if (ImGui::Button(u8"現在のピースを登録"))
    {
        RegisterCurrentPieceToHierarchy();
    }

    const char* sortModeLabels[] = { u8"デフォルト", u8"日付", u8"名前" };
    int sortMode = static_cast<int>(m_pieceHierarchySortMode);
    ImGui::SetNextItemWidth(150.0f);
    // 条件に該当する場合は、`m_pieceHierarchySortMode` の状態を更新します。
    if (ImGui::Combo(u8"並び順", &sortMode, sortModeLabels, IM_ARRAYSIZE(sortModeLabels)))
    {
        m_pieceHierarchySortMode = static_cast<PieceHierarchySortMode>(sortMode);
        m_pieceHierarchyDisplayDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(u8"降順", &m_pieceHierarchySortDescending))
    {
        m_pieceHierarchyDisplayDirty = true;
    }

    ImGui::Separator();
    // 条件に該当する場合は、`ImGui::TextUnformatted` の処理を実行します。
    if (m_pieceHierarchyEntries.empty())
    {
        ImGui::TextUnformatted(u8"登録済みの小ステージはありません");
        ImGui::End();
        return;
    }

    if (m_pieceHierarchyDisplayDirty)
    {
        RebuildPieceHierarchyDisplayCache();
    }

    const std::wstring currentPath = ToLowerWide(NormalizePathSeparators(GetCurrentEditingPieceRelativePath()));
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_pieceHierarchyDisplayEntries.size()));
    while (clipper.Step())
    {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
        {
            const PieceHierarchyDisplayEntry& displayEntry =
                m_pieceHierarchyDisplayEntries[static_cast<size_t>(index)];
            const PieceHierarchyEntry& entry = m_pieceHierarchyEntries[displayEntry.sourceIndex];
            const bool isSelected = displayEntry.normalizedRelativePath == currentPath;
            if (ImGui::Selectable(displayEntry.label.c_str(), isSelected))
            {
                if (!ConfirmDiscardDirtyChanges(L"読込"))
                {
                    continue;
                }

                const std::wstring absolutePath = ResolvePieceHierarchyPath(entry.relativePath);
                if (!PathExists(absolutePath) || !LoadPieceFromPath(absolutePath))
                {
                    HandleMissingHierarchyEntry(entry);
                    ImGui::End();
                    return;
                }
                ImGui::End();
                return;
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", displayEntry.tooltip.c_str());
            }
        }
    }

    ImGui::End();
}

std::wstring SceneNarakuPieceEditor::GetPieceHierarchyConfigPath() const
{
    EDITOR_PROFILE_FUNCTION();
    return L"Assets/Naraku/Pieces/piece_hierarchy.cfg";
}

std::wstring SceneNarakuPieceEditor::NormalizePieceHierarchyPath(const std::wstring& path) const
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedPath = NormalizePathSeparators(path);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsAbsoluteWindowsPath(normalizedPath))
    {
        return normalizedPath;
    }

    std::wstring projectRoot = GetPieceHierarchyProjectRoot();
    const std::wstring absoluteLower = ToLowerWide(normalizedPath);
    std::wstring projectRootLower = ToLowerWide(projectRoot);
    // 条件に該当する場合は、`projectRoot` の状態を更新します。
    if (!projectRootLower.empty() && projectRootLower.back() != L'/')
    {
        projectRoot += L"/";
        projectRootLower += L"/";
    }

    // 条件に該当する場合は、現在の処理をここで終了します。
    if (absoluteLower.find(projectRootLower) == 0)
    {
        return normalizedPath.substr(projectRoot.size());
    }

    return normalizedPath;
}

std::wstring SceneNarakuPieceEditor::ResolvePieceHierarchyPath(const std::wstring& relativePath) const
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedPath = NormalizePathSeparators(relativePath);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (IsAbsoluteWindowsPath(normalizedPath))
    {
        return normalizedPath;
    }

    const std::wstring projectRoot = GetPieceHierarchyProjectRoot();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (normalizedPath.empty())
    {
        return projectRoot;
    }
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!projectRoot.empty() && projectRoot.back() == L'/')
    {
        return projectRoot + normalizedPath;
    }
    return projectRoot + L"/" + normalizedPath;
}

bool SceneNarakuPieceEditor::IsCompletedPiecePath(const std::wstring& path) const
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedPath = ToLowerWide(NormalizePathSeparators(path));
    return normalizedPath.find(L"/completed/") != std::wstring::npos;
}

std::wstring SceneNarakuPieceEditor::GetCurrentEditingPieceRelativePath() const
{
    EDITOR_PROFILE_FUNCTION();
    return NormalizePieceHierarchyPath(GetCurrentSaveTargetPath());
}

bool SceneNarakuPieceEditor::ReloadPieceHierarchyEntries()
{
    EDITOR_PROFILE_FUNCTION();
    m_pieceHierarchyEntries.clear();
    m_pieceHierarchyDisplayEntries.clear();
    m_pieceHierarchyDisplayDirty = true;
    m_nextPieceHierarchyInsertionOrder = 0;
    const std::wstring configPath = ResolvePieceHierarchyPath(GetPieceHierarchyConfigPath());
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!PathExists(configPath))
    {
        SetMessage(u8"Hierarchy登録ファイルが未作成のため空一覧で開始しました");
        return true;
    }

    std::ifstream stream(configPath, std::ios::binary);
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!stream)
    {
        SetMessage(u8"Hierarchy登録ファイルを開けませんでした");
        return false;
    }

    std::string line;
    bool headerChecked = false;
    // 継続条件を満たす間、対象処理を繰り返します。
    while (std::getline(stream, line))
    {
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // 条件に該当する場合は、その要素を処理対象から除外します。
        if (line.empty())
        {
            continue;
        }
        // 条件に該当する場合は、`headerChecked` の状態を更新します。
        if (!headerChecked)
        {
            headerChecked = true;
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (line == kPieceHierarchyHeader)
            {
                continue;
            }
        }

        std::istringstream lineStream(line);
        std::string fileNameUtf8;
        std::string relativePathUtf8;
        std::string completedFlag;
        std::string lastModifiedUtf8;
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (!std::getline(lineStream, fileNameUtf8, '\t') ||
            !std::getline(lineStream, relativePathUtf8, '\t') ||
            !std::getline(lineStream, completedFlag))
        {
            continue;
        }
        std::getline(lineStream, lastModifiedUtf8);

        const std::wstring relativePath = NormalizePathSeparators(Utf8ToWide(relativePathUtf8));
        const bool isCompleted = (completedFlag == "1");
        RegisterPieceHierarchyEntry(relativePath, isCompleted, Utf8ToWide(lastModifiedUtf8));
        const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(relativePath)));
        // 対象コレクションの各要素を順に処理します。
        for (PieceHierarchyEntry& registeredEntry : m_pieceHierarchyEntries)
        {
            // 条件に該当する場合は、後続処理に必要な値を準備します。
            if (ToLowerWide(NormalizePathSeparators(registeredEntry.relativePath)) == normalizedKey)
            {
                const std::wstring explicitFileName = Utf8ToWide(fileNameUtf8);
                registeredEntry.fileName = explicitFileName.empty() ? GetFileNamePart(relativePath) : explicitFileName;
                break;
            }
        }
    }

    SetMessage(u8"Hierarchy登録ファイルを再読込しました");
    return true;
}

bool SceneNarakuPieceEditor::SavePieceHierarchyEntries() const
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring configPath = ResolvePieceHierarchyPath(GetPieceHierarchyConfigPath());
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!EnsureDirectoryExists(GetDirectoryPart(configPath)))
    {
        return false;
    }
    std::ofstream stream(configPath, std::ios::binary | std::ios::trunc);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!stream)
    {
        return false;
    }

    stream << kPieceHierarchyHeader << "\n";
    // 対象コレクションの各要素を順に処理します。
    for (const PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        stream << WideToUtf8(entry.fileName) << '\t'
            << WideToUtf8(entry.relativePath) << '\t'
            << (entry.isCompleted ? '1' : '0') << '\t'
            << WideToUtf8(entry.lastModified) << "\n";
    }
    return static_cast<bool>(stream);
}

bool SceneNarakuPieceEditor::RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted)
{
    EDITOR_PROFILE_FUNCTION();
    return RegisterPieceHierarchyEntry(path, isCompleted, std::wstring());
}

bool SceneNarakuPieceEditor::RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted, const std::wstring& lastModified)
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedPath = NormalizePieceHierarchyPath(path);
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(normalizedPath));
    const std::wstring fileName = EnsureJsonFileName(GetFileNamePart(normalizedPath));

    // 対象コレクションの各要素を順に処理します。
    for (PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        // 条件に該当する場合は、後続処理に必要な値を準備します。
        if (ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey)
        {
            const std::wstring nextLastModified = lastModified.empty() ? entry.lastModified : lastModified;
            const bool changed = entry.fileName != fileName ||
                entry.relativePath != normalizedPath ||
                entry.isCompleted != isCompleted ||
                entry.lastModified != nextLastModified;
            entry.fileName = fileName;
            entry.relativePath = normalizedPath;
            entry.isCompleted = isCompleted;
            entry.lastModified = nextLastModified;
            if (changed)
            {
                m_pieceHierarchyDisplayDirty = true;
            }
            return changed;
        }
    }

    PieceHierarchyEntry entry;
    entry.fileName = fileName;
    entry.relativePath = normalizedPath;
    entry.isCompleted = isCompleted;
    entry.lastModified = lastModified;
    entry.insertionOrder = m_nextPieceHierarchyInsertionOrder++;
    m_pieceHierarchyEntries.push_back(entry);
    m_pieceHierarchyDisplayDirty = true;
    return true;
}

bool SceneNarakuPieceEditor::RemovePieceHierarchyEntry(const std::wstring& path)
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(path)));
    const auto it = std::remove_if(
        m_pieceHierarchyEntries.begin(),
        m_pieceHierarchyEntries.end(),
        [&normalizedKey](const PieceHierarchyEntry& entry)
        {
            return ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey;
        });
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (it == m_pieceHierarchyEntries.end())
    {
        return false;
    }

    m_pieceHierarchyEntries.erase(it, m_pieceHierarchyEntries.end());
    m_pieceHierarchyDisplayDirty = true;
    return true;
}

SceneNarakuPieceEditor::PieceHierarchyEntry* SceneNarakuPieceEditor::FindPieceHierarchyEntry(const std::wstring& path)
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(path)));
    // 対象コレクションの各要素を順に処理します。
    for (PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey)
        {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<const SceneNarakuPieceEditor::PieceHierarchyEntry*> SceneNarakuPieceEditor::BuildSortedPieceHierarchyEntries() const
{
    EDITOR_PROFILE_FUNCTION();
    std::vector<const PieceHierarchyEntry*> entries;
    entries.reserve(m_pieceHierarchyEntries.size());
    // 対象コレクションの各要素を順に処理します。
    for (const PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        entries.push_back(&entry);
    }

    auto comparator = [this](const PieceHierarchyEntry* lhs, const PieceHierarchyEntry* rhs)
    {
        // 値の種類に対応する処理を選択します。
        switch (m_pieceHierarchySortMode)
        {
        case PieceHierarchySortMode::LastModified:
            // 条件に該当する場合は、現在の処理をここで終了します。
            if (lhs->lastModified != rhs->lastModified)
            {
                return lhs->lastModified < rhs->lastModified;
            }
            break;
        case PieceHierarchySortMode::FileName:
            // 条件に該当する場合は、現在の処理をここで終了します。
            if (lhs->fileName != rhs->fileName)
            {
                return lhs->fileName < rhs->fileName;
            }
            break;
        case PieceHierarchySortMode::Insertion:
        default:
            // 条件に該当する場合は、現在の処理をここで終了します。
            if (lhs->insertionOrder != rhs->insertionOrder)
            {
                return lhs->insertionOrder < rhs->insertionOrder;
            }
            break;
        }

        return lhs->relativePath < rhs->relativePath;
    };
    std::sort(entries.begin(), entries.end(), comparator);
    // 条件に該当する場合は、`std::reverse` の処理を実行します。
    if (m_pieceHierarchySortDescending)
    {
        std::reverse(entries.begin(), entries.end());
    }
    return entries;
}

void SceneNarakuPieceEditor::RebuildPieceHierarchyDisplayCache()
{
    EDITOR_PROFILE_FUNCTION();
    m_pieceHierarchyDisplayEntries.clear();
    m_pieceHierarchyDisplayEntries.reserve(m_pieceHierarchyEntries.size());

    const std::vector<const PieceHierarchyEntry*> sortedEntries = BuildSortedPieceHierarchyEntries();
    for (const PieceHierarchyEntry* entry : sortedEntries)
    {
        PieceHierarchyDisplayEntry displayEntry;
        displayEntry.sourceIndex = static_cast<size_t>(entry - m_pieceHierarchyEntries.data());
        displayEntry.normalizedRelativePath = ToLowerWide(NormalizePathSeparators(entry->relativePath));
        const std::wstring prefix =
            L"[" + BuildHierarchyDateLabel(entry->lastModified) + L" : " +
            (entry->isCompleted ? L"完成" : L"下書き") + L"] ";
        displayEntry.label = WideToUtf8(prefix + entry->fileName);
        displayEntry.tooltip = WideToUtf8(entry->relativePath);
        m_pieceHierarchyDisplayEntries.push_back(std::move(displayEntry));
    }

    m_pieceHierarchyDisplayDirty = false;
}

std::wstring SceneNarakuPieceEditor::BuildHierarchyDateLabel(const std::wstring& lastModified) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (lastModified.size() >= 10)
    {
        return lastModified.substr(0, 10);
    }
    return L"未記録";
}

void SceneNarakuPieceEditor::HandleMissingHierarchyEntry(const PieceHierarchyEntry& entry)
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring message =
        entry.fileName + L" の読込に失敗しました。Hierarchyから削除しますか?";
    const int result = ::MessageBoxW(::GetActiveWindow(), message.c_str(), L"PieceEditor", MB_ICONWARNING | MB_YESNO);
    // 条件に該当する場合は、`RemovePieceHierarchyEntry` の処理を実行します。
    if (result == IDYES)
    {
        RemovePieceHierarchyEntry(entry.relativePath);
        SavePieceHierarchyEntries();
        SetMessage(u8"読込不能なHierarchy項目を削除しました");
        return;
    }

    m_saveFileName = EnsureJsonFileName(entry.fileName);
    m_saveAsDraft = !entry.isCompleted;
    CreateNewPiece(m_saveFileName);
    m_saveAsDraft = !entry.isCompleted;
    SyncSaveFileNameInput();
    // 条件に該当する場合は、`m_piece.lastModified` の状態を更新します。
    if (!entry.lastModified.empty())
    {
        m_piece.lastModified = WideToUtf8(entry.lastModified);
    }
    SetMessage(u8"読込不能のため同名新規ピース状態へ移行しました");
}

bool SceneNarakuPieceEditor::RegisterCurrentPieceToHierarchy()
{
    EDITOR_PROFILE_FUNCTION();
    CommitSaveFileNameInput();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (m_saveFileName.empty())
    {
        SetMessage(u8"登録対象の保存ファイル名が未設定です");
        return false;
    }

    const std::wstring targetPath = GetCurrentSaveTargetPath();
    const bool changed = RegisterPieceHierarchyEntry(targetPath, !m_saveAsDraft, Utf8ToWide(m_piece.lastModified));
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"Hierarchy登録ファイルの保存に失敗しました");
        return false;
    }

    SetMessage(changed ? u8"現在のピースをHierarchyへ登録しました" : u8"現在のピースのHierarchy登録情報を更新しました");
    return true;
}


