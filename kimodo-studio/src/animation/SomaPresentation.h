#pragma once

#include "animation/Animation.h"
#include <array>
#include <string>
#include <vector>

namespace studio {

// SOMA Presentation Skeleton (expanded presentation layer, separate from SOMA30 inference backend)
struct SomaPresentationSpec {
    static const std::vector<std::string>& jointNames();
    static const std::vector<int>& parents();
    static const std::vector<std::array<float, 3>>& defaultOffsets();
    static int jointIndex(const std::string& name);
};

class SomaPresentation {
public:
    // Expand a SOMA-30 animation into the rich presentation skeleton
    static bool expandSoma30(const Animation& soma30Anim, Animation& outPresentation,
                             std::string& error);

    // Validate any animation skeleton representation
    struct ValidationResult {
        bool valid = true;
        bool isFinite = true;
        bool hierarchyValid = true;
        bool leftRightConsistent = true;
        bool standingRestPose = true;
        float standingHeight = 0.0f;
        float torsoSpan = 0.0f;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    static ValidationResult validate(const Animation& anim);
};

} // namespace studio
