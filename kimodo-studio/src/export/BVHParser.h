#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "animation/Animation.h"

namespace studio
{

class BVHParser
{
public:
    struct ValidationReport
    {
        bool valid = true;
        int joint_count = 0;
        int frame_count = 0;
        float frame_time = 0.0f;
        float fps = 0.0f;
        std::vector<std::string> joint_names;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    // Parse BVH text string into Animation struct
    [[nodiscard]] static bool ParseString(std::string_view bvh_text, Animation& out_animation, std::string& error);

    // Parse BVH file into Animation struct
    [[nodiscard]] static bool ParseFile(std::string_view file_path, Animation& out_animation, std::string& error);

    // Validate a BVH text content
    [[nodiscard]] static ValidationReport Validate(std::string_view bvh_text);

    // Convert XYZ Euler degrees to normalized Quaternion (xyzw)
    static void EulerXyzToQuat(float ex, float ey, float ez, float& x, float& y, float& z, float& w) noexcept;
};

} // namespace studio
