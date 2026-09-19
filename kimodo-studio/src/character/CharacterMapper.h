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
    static CharacterBoneMap AutoMap(const CharacterAsset& asset, const std::vector<std::string>& source_joints);

    // Compute skin matrices (globalBoneTransform * inverseBindMatrix) for an animation frame
    // Optionally outputs evaluated animated bone world positions for 1:1 skeleton alignment
    static bool EvaluateSkinMatrices(const CharacterAsset& asset, const Animation& anim, int frame,
                                     const CharacterBoneMap& mapping, std::vector<Matrix>& out_skin_matrices,
                                     std::vector<Vector3>* out_bone_positions = nullptr);
};

} // namespace studio
