#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace studio
{

enum class RetargetMode
{
    GenericLocal
};

struct SkeletonProfile
{
    std::string id;
    std::string name;
    RetargetMode mode = RetargetMode::GenericLocal;
    std::vector<std::string> joints;
    std::vector<int> parents;
    std::vector<std::pair<std::string, std::string>> default_map;
    std::vector<std::array<float, 3>> offsets;
};

[[nodiscard]] const std::vector<SkeletonProfile>& target_profiles();
[[nodiscard]] const SkeletonProfile* FindProfile(std::string_view id) noexcept;

} // namespace studio
