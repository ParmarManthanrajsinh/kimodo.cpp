#pragma once

#include "animation/Animation.h"
#include "retarget/SkeletonProfile.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace studio {

using FBoneMap = std::map<std::string, std::string>; // target joint -> source joint

struct FRetargetReport {
    std::string text;
    int mappedCount = 0;
    int unmappedCount = 0;
};

class FRetargeter {
public:
    struct Options {
        float rootScale = 1.0f;
    };

    static FBoneMap autoMap(const FSkeletonProfile& profile);
    static std::vector<std::string> unmapped(const FSkeletonProfile& profile, const FBoneMap& map);

    static bool retarget(const FAnimation& source, const FSkeletonProfile& target,
                         const FBoneMap& map, const Options& opts, FAnimation& out,
                         std::string& error, FRetargetReport* report = nullptr);
};

} // namespace studio
