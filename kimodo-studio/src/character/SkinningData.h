#pragma once

#include "raylib.h"
#include <array>
#include <cstdint>
#include <vector>

namespace studio {

inline constexpr int kMaxBones = 128;
inline constexpr int kMaxInfluences = 4;

struct FSkinVertex {
    Vector3 position{0, 0, 0};
    Vector3 normal{0, 1, 0};
    Vector2 texcoord{0, 0};
    std::array<uint16_t, kMaxInfluences> boneIndices{0, 0, 0, 0};
    std::array<float, kMaxInfluences> boneWeights{1.0f, 0.0f, 0.0f, 0.0f};
};

struct FSkinningData {
    std::vector<FSkinVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Matrix> inverseBindMatrices; // one per joint
    std::vector<Matrix> currentBoneMatrices; // skin matrices = globalTransform * invBind
    bool hasSkin = false;
};

} // namespace studio
