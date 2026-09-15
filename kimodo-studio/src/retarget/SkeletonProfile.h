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
//
// Retarget chains group ordered target joints with ordered source joints.
// A chain with equal counts transfers 1:1; a longer target span splits the
// shared source motion by rest-length weights (never duplicates a full
// source rotation across several targets).
struct ChainDef {
    std::string name;                 // "LeftLeg"
    std::vector<std::string> target;  // ordered root -> tip (profile names)
    std::vector<std::string> source;  // ordered root -> tip (SOMA names)
};

struct SkeletonProfile {
    std::string id;   // "ue5-manny"
    std::string name; // "UE5 Manny"
    std::vector<std::string> joints;
    std::vector<int> parents;
    // Default source joint per target joint (alias table, explicit).
    std::vector<std::pair<std::string, std::string>> defaultMap;
    bool hasBind = false;
    std::vector<std::array<float, 3>> offsets; // rest offsets (meters)
    std::vector<std::array<float, 4>> restLocal; // rest-local quats xyzw
    std::vector<ChainDef> chains; // empty = legacy per-bone copy path
};

const std::vector<SkeletonProfile>& targetProfiles();
const SkeletonProfile* findProfile(const std::string& id);

} // namespace studio
