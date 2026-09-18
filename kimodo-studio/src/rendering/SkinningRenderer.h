#pragma once

#include "character/CharacterAsset.h"
#include "raylib.h"
#include <vector>

namespace studio {

class FSkinningRenderer {
public:
    FSkinningRenderer() = default;
    ~FSkinningRenderer();

    bool Init();
    void Shutdown();

    // Render character mesh with given skin matrices
    void drawCharacter(FCharacterAsset& character,
                       const std::vector<Matrix>& skinMatrices,
                       bool wireframe);

private:
    bool bInitialized = false;
    Shader skinShader{};
    int BoneMatricesLoc = -1;
};

} // namespace studio
