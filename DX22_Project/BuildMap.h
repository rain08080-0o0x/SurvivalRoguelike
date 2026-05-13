#pragma once

#include "CastlePlacementInfo.h"

#include <vector>

class BuildMap
{
public:
    int GetPlacementCount() const;
    const CastlePlacementInfo* GetPlacement(int index) const;
    int InsertPlacement(const CastlePlacementInfo& placement, int index);
    void RemovePlacement(int index);
    void UpdatePlacement(int index, const CastlePlacementInfo& placement);
    int FindPlacementIndexAt(int gridX, int gridY, int gridZ) const;
    const CastlePlacementInfo* FindPlacementAt(int gridX, int gridY, int gridZ) const;
    bool HasPlacementAt(int gridX, int gridY, int gridZ) const;
    void Clear();
    void SetPlacements(const std::vector<CastlePlacementInfo>& placements);
    const std::vector<CastlePlacementInfo>& GetPlacements() const;

private:
    std::vector<CastlePlacementInfo> m_placements;
};
