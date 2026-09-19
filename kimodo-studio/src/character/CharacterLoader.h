#pragma once

#include <string>
#include "character/CharacterAsset.h"

namespace studio {

class CharacterLoader {
public:
    // Load a glTF or GLB 3D humanoid character
    static bool LoadGLB(const std::string& file_path, CharacterAsset& out_asset, std::string& error);

    // Validate a loaded character asset
    static CharacterValidationReport Validate(const CharacterAsset& asset);
};

} // namespace studio
