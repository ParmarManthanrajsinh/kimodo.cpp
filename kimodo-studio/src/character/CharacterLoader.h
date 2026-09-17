#pragma once

#include "character/CharacterAsset.h"
#include <string>

namespace studio {

class CharacterLoader {
public:
    // Load a glTF or GLB 3D humanoid character
    static bool loadGLB(const std::string& filePath, CharacterAsset& outAsset,
                        std::string& error);

    // Validate a loaded character asset
    static CharacterValidationReport validate(const CharacterAsset& asset);
};

} // namespace studio
