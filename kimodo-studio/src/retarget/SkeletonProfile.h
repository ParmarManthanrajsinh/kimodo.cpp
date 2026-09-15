#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace studio {

// Target skeleton topology. Profiles with hasBind carry their own rest pose
// (offsets + rest-local quats, e.g. extracted from the real DCC skeleton);
// others transfer source offsets through the joint map (rotation retarget,
// proportions preserved).
struct SkeletonProfile {
    std::string id;   // "unreal-manny"
    std::string name; // "Manny Mixamo (UE)"
    std::vector<std::string> joints;
    std::vector<int> parents;
    // Default source joint per target joint (alias table, explicit).
    std::vector<std::pair<std::string, std::string>> defaultMap;
    bool hasBind = false;
    std::vector<std::array<float, 3>> offsets; // rest offsets (meters)
    std::vector<std::array<float, 4>> restLocal; // rest-local quats xyzw
};

const std::vector<SkeletonProfile>& targetProfiles();
const SkeletonProfile* findProfile(const std::string& id);

} // namespace studio
