#pragma once

#include "character/CharacterAsset.h"
#include <string>

namespace studio {

class FCharacterLoader {
public:
    // Load a glTF or GLB 3D humanoid character
    static bool loadGLB(const std::string& filePath, FCharacterAsset& outAsset,
                        std::string& error);

    // Validate a loaded character asset
    static FCharacterValidationReport validate(const FCharacterAsset& asset);
};

} // namespace studio
