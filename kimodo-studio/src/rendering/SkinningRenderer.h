#pragma once

#include "character/CharacterAsset.h"
#include "raylib.h"
#include <vector>

namespace studio {

class SkinningRenderer {
public:
    SkinningRenderer() = default;
    ~SkinningRenderer();

    bool init();
    void shutdown();

    // Render character mesh with given skin matrices
    void drawCharacter(CharacterAsset& character,
                       const std::vector<Matrix>& skinMatrices,
                       bool wireframe);

private:
    bool initialized_ = false;
    Shader skinShader_{};
    int boneMatricesLoc_ = -1;
};

} // namespace studio
