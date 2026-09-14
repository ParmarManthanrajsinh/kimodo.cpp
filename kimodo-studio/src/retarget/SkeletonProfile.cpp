#include "retarget/SkeletonProfile.h"

namespace studio {

const std::vector<SkeletonProfile>& targetProfiles() {
    static const std::vector<SkeletonProfile> profiles = [] {
        std::vector<SkeletonProfile> out;

        SkeletonProfile manny;
        manny.id = "unreal-manny";
        manny.name = "Unreal Manny (UE5)";
        manny.joints = {"pelvis",      "spine_01", "spine_02",   "spine_03",
                        "neck_01",     "head",     "clavicle_l", "upperarm_l",
                        "forearm_l",   "hand_l",   "clavicle_r", "upperarm_r",
                        "forearm_r",   "hand_r",   "thigh_l",    "calf_l",
                        "foot_l",      "ball_l",   "thigh_r",    "calf_r",
                        "foot_r",      "ball_r"};
        manny.parents = {-1, 0, 1, 2, 3, 4, 3, 6, 7, 8, 3, 10, 11, 12,
                         0, 14, 15, 16, 0, 18, 19, 20};
        manny.defaultMap = {
            {"pelvis", "Hips"},         {"spine_01", "Spine1"},
            {"spine_02", "Spine2"},     {"spine_03", "Chest"},
            {"neck_01", "Neck1"},       {"head", "Head"},
            {"clavicle_l", "LeftShoulder"}, {"upperarm_l", "LeftArm"},
            {"forearm_l", "LeftForeArm"}, {"hand_l", "LeftHand"},
            {"clavicle_r", "RightShoulder"}, {"upperarm_r", "RightArm"},
            {"forearm_r", "RightForeArm"}, {"hand_r", "RightHand"},
            {"thigh_l", "LeftLeg"},     {"calf_l", "LeftShin"},
            {"foot_l", "LeftFoot"},     {"ball_l", "LeftToeBase"},
            {"thigh_r", "RightLeg"},    {"calf_r", "RightShin"},
            {"foot_r", "RightFoot"},    {"ball_r", "RightToeBase"},
        };
        out.push_back(std::move(manny));

        SkeletonProfile humanoid;
        humanoid.id = "unity-humanoid";
        humanoid.name = "Unity Humanoid";
        humanoid.joints = {"Hips",          "Spine",         "Chest",
                           "Neck",          "Head",          "LeftShoulder",
                           "LeftUpperArm",  "LeftLowerArm",  "LeftHand",
                           "RightShoulder", "RightUpperArm", "RightLowerArm",
                           "RightHand",     "LeftUpperLeg",  "LeftLowerLeg",
                           "LeftFoot",      "LeftToes",      "RightUpperLeg",
                           "RightLowerLeg", "RightFoot",     "RightToes"};
        humanoid.parents = {-1, 0, 1, 2, 3, 2, 5, 6, 7, 2, 9, 10, 11,
                            0, 13, 14, 15, 0, 17, 18, 19};
        humanoid.defaultMap = {
            {"Hips", "Hips"},           {"Spine", "Spine1"},
            {"Chest", "Spine2"},        {"Neck", "Neck1"},
            {"Head", "Head"},           {"LeftShoulder", "LeftShoulder"},
            {"LeftUpperArm", "LeftArm"}, {"LeftLowerArm", "LeftForeArm"},
            {"LeftHand", "LeftHand"},   {"RightShoulder", "RightShoulder"},
            {"RightUpperArm", "RightArm"}, {"RightLowerArm", "RightForeArm"},
            {"RightHand", "RightHand"}, {"LeftUpperLeg", "LeftLeg"},
            {"LeftLowerLeg", "LeftShin"}, {"LeftFoot", "LeftFoot"},
            {"LeftToes", "LeftToeBase"}, {"RightUpperLeg", "RightLeg"},
            {"RightLowerLeg", "RightShin"}, {"RightFoot", "RightFoot"},
            {"RightToes", "RightToeBase"},
        };
        out.push_back(std::move(humanoid));

        SkeletonProfile blender;
        blender.id = "blender-generic";
        blender.name = "Blender (generic)";
        blender.joints = {"Hips", "Spine1", "Spine2", "Chest", "Neck1", "Neck2",
                          "Head", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
                          "RightShoulder", "RightArm", "RightForeArm", "RightHand",
                          "LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase",
                          "RightLeg", "RightShin", "RightFoot", "RightToeBase"};
        blender.parents = {-1, 0, 1, 2, 3, 4, 5, 3, 7, 8, 9, 3,
                           11, 12, 13, 0, 15, 16, 17, 0, 19, 20, 21};
        for (const std::string& j : blender.joints) {
            blender.defaultMap.emplace_back(j, j);
        }
        out.push_back(std::move(blender));

        return out;
    }();
    return profiles;
}

const SkeletonProfile* findProfile(const std::string& id) {
    for (const SkeletonProfile& p : targetProfiles()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace studio
