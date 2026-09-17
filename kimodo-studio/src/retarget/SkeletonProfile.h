#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace studio {

enum class RetargetMode {
    GenericLocal
};

struct SkeletonProfile {
    std::string id;
    std::string name;
    RetargetMode mode = RetargetMode::GenericLocal;
    std::vector<std::string> joints;
    std::vector<int> parents;
    std::vector<std::pair<std::string, std::string>> defaultMap;
    std::vector<std::array<float, 3>> offsets;
};

const std::vector<SkeletonProfile>& targetProfiles();
const SkeletonProfile* findProfile(const std::string& id);

} // namespace studio
