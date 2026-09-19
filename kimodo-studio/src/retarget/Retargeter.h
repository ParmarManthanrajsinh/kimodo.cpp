#pragma once

#include "animation/Animation.h"
#include "retarget/SkeletonProfile.h"

#include <map>
#include <string>
#include <vector>

namespace studio {

using BoneMap = std::map<std::string, std::string>; // target joint -> source joint

struct RetargetReport {
    std::string text;
    int mapped_count = 0;
    int unmapped_count = 0;
};

class Retargeter {
public:
    struct Options {
        float root_scale = 1.0f;
    };

    static BoneMap AutoMap(const SkeletonProfile& profile);
    static std::vector<std::string> unmapped(const SkeletonProfile& profile, const BoneMap& map);

    static bool retarget(const Animation& source, const SkeletonProfile& target, const BoneMap& map,
                         const Options& opts, Animation& out, std::string& error, RetargetReport* report = nullptr);
};

} // namespace studio
