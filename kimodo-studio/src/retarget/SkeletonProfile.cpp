#include "retarget/SkeletonProfile.h"

namespace studio
{

const std::vector<SkeletonProfile>& target_profiles()
{
    static const std::vector<SkeletonProfile> profiles = [] {
        std::vector<SkeletonProfile> out;

        // Blender Generic Humanoid Profile
        SkeletonProfile blender;
        blender.id = "blender-generic";
        blender.name = "Blender (generic)";
        blender.mode = RetargetMode::GenericLocal;
        blender.joints = {"Hips",        "Spine1",       "Spine2",    "Chest",       "Neck1",       "Neck2",
                          "Head",        "LeftShoulder", "LeftArm",   "LeftForeArm", "LeftHand",    "RightShoulder",
                          "RightArm",    "RightForeArm", "RightHand", "LeftLeg",     "LeftShin",    "LeftFoot",
                          "LeftToeBase", "RightLeg",     "RightShin", "RightFoot",   "RightToeBase"};
        blender.parents = {-1, 0, 1, 2, 3, 4, 5, 3, 7, 8, 9, 3, 11, 12, 13, 0, 15, 16, 17, 0, 19, 20, 21};
        for (const std::string& j : blender.joints)
        {
            blender.default_map.emplace_back(j, j);
        }
        out.push_back(std::move(blender));

        // Generic Humanoid Profile (for general DCC & BVH pipelines)
        SkeletonProfile generic;
        generic.id = "generic-humanoid";
        generic.name = "Generic Humanoid";
        generic.mode = RetargetMode::GenericLocal;
        generic.joints = {"Hips",         "Spine",     "Spine1",      "Spine2",      "Neck",          "Head",
                          "LeftShoulder", "LeftArm",   "LeftForeArm", "LeftHand",    "RightShoulder", "RightArm",
                          "RightForeArm", "RightHand", "LeftUpLeg",   "LeftLeg",     "LeftFoot",      "LeftToeBase",
                          "RightUpLeg",   "RightLeg",  "RightFoot",   "RightToeBase"};
        generic.parents = {-1, 0, 1, 2, 3, 4, 3, 6, 7, 8, 3, 10, 11, 12, 0, 14, 15, 16, 0, 18, 19, 20};
        generic.default_map = {{"Hips", "Hips"},
                               {"Spine", "Spine1"},
                               {"Spine1", "Spine2"},
                               {"Spine2", "Chest"},
                               {"Neck", "Neck1"},
                               {"Head", "Head"},
                               {"LeftShoulder", "LeftShoulder"},
                               {"LeftArm", "LeftArm"},
                               {"LeftForeArm", "LeftForeArm"},
                               {"LeftHand", "LeftHand"},
                               {"RightShoulder", "RightShoulder"},
                               {"RightArm", "RightArm"},
                               {"RightForeArm", "RightForeArm"},
                               {"RightHand", "RightHand"},
                               {"LeftUpLeg", "LeftLeg"},
                               {"LeftLeg", "LeftShin"},
                               {"LeftFoot", "LeftFoot"},
                               {"LeftToeBase", "LeftToeBase"},
                               {"RightUpLeg", "RightLeg"},
                               {"RightLeg", "RightShin"},
                               {"RightFoot", "RightFoot"},
                               {"RightToeBase", "RightToeBase"}};
        out.push_back(std::move(generic));

        return out;
    }();
    return profiles;
}

const SkeletonProfile* FindProfile(const std::string& id)
{
    for (const SkeletonProfile& p : target_profiles())
    {
        if (p.id == id)
        {
            return &p;
        }
    }
    return nullptr;
}

} // namespace studio
