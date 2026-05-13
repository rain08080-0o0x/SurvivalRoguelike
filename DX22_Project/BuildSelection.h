#pragma once

#include <vector>

class BuildSelection
{
public:
    int GetSelectedPlacementIndex() const;
    int GetSelectedPlacementCount() const;
    int GetSelectedPlacementIndexAt(int order) const;
    const std::vector<int>& GetSelectedPlacementIndices() const;
    bool HasSelection() const;
    bool IsSelected(int index) const;
    void ClearSelection();
    void SetSelectedPlacementIndex(int index, int placementCount);
    void AddSelectedPlacementIndex(int index, int placementCount);
    void ToggleSelectedPlacementIndex(int index, int placementCount);
    void SetSelectedPlacementIndices(const std::vector<int>& indices, int placementCount);
    void ClampSelection(int placementCount);
    void OnPlacementInserted(int index);
    void OnPlacementRemoved(int index, int placementCountAfterRemoval);

private:
    void SortAndUnique();

private:
    std::vector<int> m_selectedPlacementIndices;
};
