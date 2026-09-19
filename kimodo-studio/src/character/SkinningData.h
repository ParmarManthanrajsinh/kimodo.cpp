#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "raylib.h"

namespace studio {

inline constexpr int kMaxBones = 128;
inline constexpr int kMaxInfluences = 4;

struct SkinVertex {
    Vector3 position{0, 0, 0};
    Vector3 normal{0, 1, 0};
    Vector2 texcoord{0, 0};
    std::array<uint16_t, kMaxInfluences> bone_indices{0, 0, 0, 0};
    std::array<float, kMaxInfluences> bone_weights{1.0f, 0.0f, 0.0f, 0.0f};
};

struct SkinningData {
    std::vector<SkinVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Matrix> inverse_bind_matrices; // one per joint
    std::vector<Matrix> current_bone_matrices; // skin matrices = globalTransform * invBind
    bool has_skin = false;
};

} // namespace studio
