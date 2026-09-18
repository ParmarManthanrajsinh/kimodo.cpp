#pragma once

#include "animation/Animation.h"
#include "character/CharacterAsset.h"
#include "raylib.h"

#include <map>
#include <string>
#include <vector>

namespace studio {

// Mapping from Character bone name -> Source animation joint name
using FCharacterBoneMap = std::map<std::string, std::string>;

class FCharacterMapper {
public:
    // Auto-map character bones to SOMA/humanoid animation joints using alias tables
    static FCharacterBoneMap autoMap(const FCharacterAsset& asset,
                                    const std::vector<std::string>& sourceJoints);

    // Compute skin matrices (globalBoneTransform * inverseBindMatrix) for an animation frame
    // Optionally outputs evaluated animated bone world positions for 1:1 skeleton alignment
    static bool evaluateSkinMatrices(const FCharacterAsset& asset,
                                     const FAnimation& anim,
                                     int frame,
                                     const FCharacterBoneMap& mapping,
                                     std::vector<Matrix>& outSkinMatrices,
                                     std::vector<Vector3>* outBonePositions = nullptr);
};

} // namespace studio
