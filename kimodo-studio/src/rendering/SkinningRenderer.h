#pragma once

#include <vector>
#include "character/CharacterAsset.h"
#include "raylib.h"

namespace studio
{

class SkinningRenderer
{
public:
    SkinningRenderer() = default;
    bool Init() noexcept { return true; }
    void Shutdown() noexcept {}

    // Render character mesh with given skin matrices
    void DrawCharacter(CharacterAsset& character, const std::vector<Matrix>& skin_matrices, bool wireframe);
};

} // namespace studio
