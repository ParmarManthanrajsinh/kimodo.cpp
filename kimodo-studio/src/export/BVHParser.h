#pragma once

#include "animation/Animation.h"
#include <string>
#include <vector>

namespace studio {

class FBVHParser {
public:
    struct ValidationReport {
        bool valid = true;
        int jointCount = 0;
        int frameCount = 0;
        float frameTime = 0.0f;
        float fps = 0.0f;
        std::vector<std::string> jointNames;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    // Parse BVH text string into Animation struct
    static bool parseString(const std::string& bvhText, FAnimation& outAnimation,
                            std::string& error);

    // Parse BVH file into Animation struct
    static bool parseFile(const std::string& filePath, FAnimation& outAnimation,
                          std::string& error);

    // Validate a BVH text content
    static ValidationReport validate(const std::string& bvhText);

    // Convert XYZ Euler degrees to normalized Quaternion (xyzw)
    static void eulerXYZToQuat(float ex, float ey, float ez,
                               float& x, float& y, float& z, float& w);
};

} // namespace studio
