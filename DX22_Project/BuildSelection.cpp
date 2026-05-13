#include "BuildSelection.h"

#include <algorithm>

int BuildSelection::GetSelectedPlacementIndex() const
{
    if (m_selectedPlacementIndices.empty())
    {
        return -1;
    }

    return m_selectedPlacementIndices.back();
}

int BuildSelection::GetSelectedPlacementCount() const
{
    return static_cast<int>(m_selectedPlacementIndices.size());
}

int BuildSelection::GetSelectedPlacementIndexAt(int order) const
{
    if (order < 0 || order >= static_cast<int>(m_selectedPlacementIndices.size()))
    {
        return -1;
    }

    return m_selectedPlacementIndices[static_cast<size_t>(order)];
}

const std::vector<int>& BuildSelection::GetSelectedPlacementIndices() const
{
    return m_selectedPlacementIndices;
}

bool BuildSelection::HasSelection() const
{
    return !m_selectedPlacementIndices.empty();
}

bool BuildSelection::IsSelected(int index) const
{
    return std::find(m_selectedPlacementIndices.begin(), m_selectedPlacementIndices.end(), index) != m_selectedPlacementIndices.end();
}

void BuildSelection::ClearSelection()
{
    m_selectedPlacementIndices.clear();
}

void BuildSelection::SetSelectedPlacementIndex(int index, int placementCount)
{
    if (index < 0)
    {
        ClearSelection();
        return;
    }

    if (index >= placementCount)
    {
        return;
    }

    m_selectedPlacementIndices.assign(1, index);
}

void BuildSelection::AddSelectedPlacementIndex(int index, int placementCount)
{
    if (index < 0 || index >= placementCount)
    {
        return;
    }

    if (!IsSelected(index))
    {
        m_selectedPlacementIndices.push_back(index);
        SortAndUnique();
    }
}

void BuildSelection::ToggleSelectedPlacementIndex(int index, int placementCount)
{
    if (index < 0 || index >= placementCount)
    {
        return;
    }

    const auto it = std::find(m_selectedPlacementIndices.begin(), m_selectedPlacementIndices.end(), index);
    if (it != m_selectedPlacementIndices.end())
    {
        m_selectedPlacementIndices.erase(it);
        return;
    }

    m_selectedPlacementIndices.push_back(index);
    SortAndUnique();
}

void BuildSelection::SetSelectedPlacementIndices(const std::vector<int>& indices, int placementCount)
{
    m_selectedPlacementIndices.clear();
    for (int index : indices)
    {
        if (index < 0 || index >= placementCount) continue;
        m_selectedPlacementIndices.push_back(index);
    }
    SortAndUnique();
}

void BuildSelection::ClampSelection(int placementCount)
{
    m_selectedPlacementIndices.erase(
        std::remove_if(
            m_selectedPlacementIndices.begin(),
            m_selectedPlacementIndices.end(),
            [&](int index) { return index < 0 || index >= placementCount; }),
        m_selectedPlacementIndices.end());
}

void BuildSelection::OnPlacementInserted(int index)
{
    for (int& selectedIndex : m_selectedPlacementIndices)
    {
        if (selectedIndex >= index)
        {
            ++selectedIndex;
        }
    }
}

void BuildSelection::OnPlacementRemoved(int index, int placementCountAfterRemoval)
{
    for (int& selectedIndex : m_selectedPlacementIndices)
    {
        if (selectedIndex == index)
        {
            selectedIndex = -1;
        }
        else if (selectedIndex > index)
        {
            --selectedIndex;
        }
    }

    ClampSelection(placementCountAfterRemoval);
}

void BuildSelection::SortAndUnique()
{
    std::sort(m_selectedPlacementIndices.begin(), m_selectedPlacementIndices.end());
    m_selectedPlacementIndices.erase(
        std::unique(m_selectedPlacementIndices.begin(), m_selectedPlacementIndices.end()),
        m_selectedPlacementIndices.end());
}
