#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace studio {

// Target skeleton topology. Rest offsets are NOT stored here: v1 retarget
// transfers the source rest offset through the joint map (rotation retarget,
// proportions preserved). Engine-specific rest poses come later.
struct SkeletonProfile {
    std::string id;   // "unreal-manny"
    std::string name; // "Unreal Manny (UE5)"
    std::vector<std::string> joints;
    std::vector<int> parents;
    // Default source joint per target joint (alias table, explicit).
    std::vector<std::pair<std::string, std::string>> defaultMap;
};

const std::vector<SkeletonProfile>& targetProfiles();
const SkeletonProfile* findProfile(const std::string& id);

} // namespace studio
