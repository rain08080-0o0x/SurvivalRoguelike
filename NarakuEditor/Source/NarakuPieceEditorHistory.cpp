#include "NarakuPieceEditorHistory.h"

#include "EditorPerformanceProfiler.h"

void NarakuPieceEditorHistory::Push(const Snapshot& snapshot)
{
    EDITOR_PROFILE_FUNCTION();
    m_undoStack.push_back(snapshot);
    Trim(m_undoStack);
    m_redoStack.clear();
}

bool NarakuPieceEditorHistory::TryUndo(const Snapshot& currentSnapshot, Snapshot& outSnapshot)
{
    EDITOR_PROFILE_FUNCTION();
    // Undo対象が無い場合は現在状態を変更しません。
    if (m_undoStack.empty())
    {
        return false;
    }

    m_redoStack.push_back(currentSnapshot);
    Trim(m_redoStack);
    outSnapshot = m_undoStack.back();
    m_undoStack.pop_back();
    return true;
}

bool NarakuPieceEditorHistory::TryRedo(const Snapshot& currentSnapshot, Snapshot& outSnapshot)
{
    EDITOR_PROFILE_FUNCTION();
    // Redo対象が無い場合は現在状態を変更しません。
    if (m_redoStack.empty())
    {
        return false;
    }

    m_undoStack.push_back(currentSnapshot);
    Trim(m_undoStack);
    outSnapshot = m_redoStack.back();
    m_redoStack.pop_back();
    return true;
}

void NarakuPieceEditorHistory::Clear()
{
    EDITOR_PROFILE_FUNCTION();
    m_undoStack.clear();
    m_redoStack.clear();
}

bool NarakuPieceEditorHistory::CanUndo() const
{
    EDITOR_PROFILE_FUNCTION();
    return !m_undoStack.empty();
}

bool NarakuPieceEditorHistory::CanRedo() const
{
    EDITOR_PROFILE_FUNCTION();
    return !m_redoStack.empty();
}

size_t NarakuPieceEditorHistory::GetUndoCount() const
{
    EDITOR_PROFILE_FUNCTION();
    return m_undoStack.size();
}

size_t NarakuPieceEditorHistory::GetRedoCount() const
{
    EDITOR_PROFILE_FUNCTION();
    return m_redoStack.size();
}

void NarakuPieceEditorHistory::Trim(std::vector<Snapshot>& stack)
{
    EDITOR_PROFILE_FUNCTION();
    // 保持上限を超えた古い履歴だけを先頭から破棄します。
    if (stack.size() <= kMaximumHistoryCount)
    {
        return;
    }

    const size_t overflowCount = stack.size() - kMaximumHistoryCount;
    stack.erase(stack.begin(), stack.begin() + overflowCount);
}
