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
    ~SkinningRenderer();

    bool Init();
    void Shutdown();

    // Render character mesh with given skin matrices
    void DrawCharacter(CharacterAsset& character, const std::vector<Matrix>& skin_matrices, bool wireframe);

private:
    bool initialized = false;
    Shader skinShader{};
    int BoneMatricesLoc = -1;
};

} // namespace studio
