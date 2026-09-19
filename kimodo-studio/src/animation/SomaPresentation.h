#pragma once

#include <array>
#include <string>
#include <vector>
#include "animation/Animation.h"

namespace studio {

// SOMA Presentation Skeleton (expanded presentation layer, separate from SOMA30 inference backend)
struct SomaPresentationSpec {
    static const std::vector<std::string>& joint_names();
    static const std::vector<int>& parents();
    static const std::vector<std::array<float, 3>>& default_offsets();
    static int joint_index(const std::string& name);
};

class SomaPresentation {
public:
    // Expand a SOMA-30 animation into the rich presentation skeleton
    static bool ExpandSoma30(const Animation& soma30Anim, Animation& out_presentation, std::string& error);

    // Validate any animation skeleton representation
    struct ValidationResult {
        bool valid = true;
        bool is_finite = true;
        bool hierarchy_valid = true;
        bool leftRightConsistent = true;
        bool standing_rest_pose = true;
        float standing_height = 0.0f;
        float torso_span = 0.0f;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    static ValidationResult Validate(const Animation& anim);
};

} // namespace studio
