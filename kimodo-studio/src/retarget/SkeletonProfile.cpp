#include "retarget/SkeletonProfile.h"

#include "retarget/manny_true.inc"

namespace studio {

const std::vector<SkeletonProfile>& targetProfiles() {
    static const std::vector<SkeletonProfile> profiles = [] {
        std::vector<SkeletonProfile> out;

        // True Manny rest pose extracted from user FBX Lcl values
        // (see manny_true.inc header). Units meters.
        SkeletonProfile manny;
        manny.id = "unreal-manny";
        manny.name = "Manny Mixamo (UE)";
        manny.hasBind = true;
        for (int i = 0; i < kMannyTrueJoints; ++i) {
            manny.joints.emplace_back(kMannyTrueNames[i]);
            manny.parents.push_back(kMannyTrueParents[i]);
            manny.offsets.push_back({kMannyTrueOffsets[i][0], kMannyTrueOffsets[i][1],
                                     kMannyTrueOffsets[i][2]});
            manny.restLocal.push_back({kMannyTrueRest[i][0], kMannyTrueRest[i][1],
                                       kMannyTrueRest[i][2], kMannyTrueRest[i][3]});
        }
        manny.defaultMap = {
            {"Hips", "Hips"},
            {"Spine", "Spine1"},
            {"Spine1", "Spine2"},
            {"Spine2", "Chest"},
            {"Spine3", "Chest"},
            {"Neck", "Neck1"},
            {"Neck1", "Neck2"},
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
            {"RightToeBase", "RightToeBase"},
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
