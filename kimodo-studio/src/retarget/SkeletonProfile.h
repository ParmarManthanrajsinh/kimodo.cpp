#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace studio {

enum class ERetargetMode {
    GenericLocal
};

struct FSkeletonProfile {
    std::string id;
    std::string name;
    ERetargetMode mode = ERetargetMode::GenericLocal;
    std::vector<std::string> joints;
    std::vector<int> parents;
    std::vector<std::pair<std::string, std::string>> defaultMap;
    std::vector<std::array<float, 3>> offsets;
};

const std::vector<FSkeletonProfile>& targetProfiles();
const FSkeletonProfile* FindProfile(const std::string& id);

} // namespace studio
