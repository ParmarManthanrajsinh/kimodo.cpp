#pragma once

#include <map>
#include <string>
#include <vector>

#include "animation/Animation.h"

namespace studio {

struct SkeletonProfile;

// target joint -> source joint ("(none)" = identity bind).
using BoneMap = std::map<std::string, std::string>;

class Retargeter {
public:
    // Default alias map for a profile.
    static BoneMap autoMap(const SkeletonProfile& target);

    // Unmapped target joints (need manual mapping).
    static std::vector<std::string> unmapped(const SkeletonProfile& target,
                                             const BoneMap& map);

    struct Options {
        float rootScale = 1.0f; // root translation scale into target units
    };

    // Rotation retarget: mapped joints copy source local quats, unmapped get
    // identity; rest offsets transfer through the map; roots scaled.
    static bool retarget(const Animation& source, const SkeletonProfile& target,
                         const BoneMap& map, const Options& opts, Animation& out,
                         std::string& error);
};

} // namespace studio
