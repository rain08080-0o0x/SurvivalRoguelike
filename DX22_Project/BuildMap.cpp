#include "BuildMap.h"

namespace
{
    bool MatchesGridCell(const CastlePlacementInfo& placement, int gridX, int gridY, int gridZ)
    {
        return placement.gridX == gridX &&
            placement.gridY == gridY &&
            placement.gridZ == gridZ;
    }
}

int BuildMap::GetPlacementCount() const
{
    return static_cast<int>(m_placements.size());
}

const CastlePlacementInfo* BuildMap::GetPlacement(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_placements.size())) return nullptr;
    return &m_placements[static_cast<size_t>(index)];
}

int BuildMap::InsertPlacement(const CastlePlacementInfo& placement, int index)
{
    if (index < 0 || index > static_cast<int>(m_placements.size()))
    {
        index = static_cast<int>(m_placements.size());
    }

    m_placements.insert(m_placements.begin() + index, placement);
    return index;
}

void BuildMap::RemovePlacement(int index)
{
    if (index < 0 || index >= static_cast<int>(m_placements.size())) return;
    m_placements.erase(m_placements.begin() + index);
}

void BuildMap::UpdatePlacement(int index, const CastlePlacementInfo& placement)
{
    if (index < 0 || index >= static_cast<int>(m_placements.size())) return;
    m_placements[static_cast<size_t>(index)] = placement;
}

int BuildMap::FindPlacementIndexAt(int gridX, int gridY, int gridZ) const
{
    for (size_t i = 0; i < m_placements.size(); ++i)
    {
        const CastlePlacementInfo& placement = m_placements[i];
        if (MatchesGridCell(placement, gridX, gridY, gridZ))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

const CastlePlacementInfo* BuildMap::FindPlacementAt(int gridX, int gridY, int gridZ) const
{
    const int index = FindPlacementIndexAt(gridX, gridY, gridZ);
    return GetPlacement(index);
}

bool BuildMap::HasPlacementAt(int gridX, int gridY, int gridZ) const
{
    return FindPlacementIndexAt(gridX, gridY, gridZ) != -1;
}

void BuildMap::Clear()
{
    m_placements.clear();
}

void BuildMap::SetPlacements(const std::vector<CastlePlacementInfo>& placements)
{
    m_placements = placements;
}

const std::vector<CastlePlacementInfo>& BuildMap::GetPlacements() const
{
    return m_placements;
}
