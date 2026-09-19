#pragma once

#include "animation/Animation.h"
#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"
#include "export/AnimationExporter.h"

#include <string>

namespace studio {

class CharacterGLBExporter {
public:
    // Export full character (Mesh + Skin + Skeleton + Animation) to binary glTF (.glb)
    static bool ExportCharacterGLB(const CharacterAsset& character, const Animation& animation,
                                   const CharacterBoneMap& mapping, const ExportOptions& options, std::string& error,
                                   std::string* report = nullptr);
};

} // namespace studio
