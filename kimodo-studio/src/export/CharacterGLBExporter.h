#pragma once

#include "animation/Animation.h"
#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"
#include "export/AnimationExporter.h"

#include <string>

namespace studio {

class FCharacterGLBExporter {
public:
    // Export full character (Mesh + Skin + Skeleton + Animation) to binary glTF (.glb)
    static bool exportCharacterGLB(const FCharacterAsset& character,
                                   const FAnimation& animation,
                                   const FCharacterBoneMap& mapping,
                                   const FExportOptions& options,
                                   std::string& error,
                                   std::string* report = nullptr);
};

} // namespace studio
