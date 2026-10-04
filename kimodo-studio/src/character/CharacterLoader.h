#pragma once

#include <string>
#include <string_view>
#include "character/CharacterAsset.h"

namespace studio
{

class CharacterLoader
{
public:
    // Load a glTF or GLB 3D humanoid character
    [[nodiscard]] static bool LoadGLB(std::string_view file_path, CharacterAsset& out_asset, std::string& error);

    // Validate a loaded character asset
    [[nodiscard]] static CharacterValidationReport Validate(const CharacterAsset& asset);
};

} // namespace studio
