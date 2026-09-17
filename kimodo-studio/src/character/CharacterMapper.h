#pragma once

#include "animation/Animation.h"
#include "character/CharacterAsset.h"
#include "raylib.h"

#include <map>
#include <string>
#include <vector>

namespace studio {

// Mapping from Character bone name -> Source animation joint name
using CharacterBoneMap = std::map<std::string, std::string>;

class CharacterMapper {
public:
    // Auto-map character bones to SOMA/humanoid animation joints using alias tables
    static CharacterBoneMap autoMap(const CharacterAsset& asset,
                                    const std::vector<std::string>& sourceJoints);

    // Compute skin matrices (globalBoneTransform * inverseBindMatrix) for an animation frame
    static bool evaluateSkinMatrices(const CharacterAsset& asset,
                                     const Animation& anim,
                                     int frame,
                                     const CharacterBoneMap& mapping,
                                     std::vector<Matrix>& outSkinMatrices);
};

} // namespace studio
