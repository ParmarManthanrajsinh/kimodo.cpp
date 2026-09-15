#include "retarget/SkeletonProfile.h"

#include "animation/Skeleton.h"
#include "raymath.h"
#include "retarget/manny_true.inc"

#include <map>

namespace studio {
namespace {

// Core-only filter: fingers, metacarpals, twist bones and end nubs never
// map from SOMA (no source joints) and only bloat tables/exports.
bool keepMannyJoint(const std::string& name) {
    for (const char* drop : {"Thumb", "Index", "Middle", "Ring", "Pinky", "metacarpal",
                             "twist", "HeadTop", "Eye", "Jaw", "_End"}) {
        if (name.find(drop) != std::string::npos) {
            return false;
        }
    }
    return true;
}

} // namespace

const std::vector<SkeletonProfile>& targetProfiles() {
    static const std::vector<SkeletonProfile> profiles = [] {
        std::vector<SkeletonProfile> out;

        // True Manny rest pose extracted from user FBX Lcl values
        // (see manny_true.inc header). Units meters. Reduced to the core
        // body: rest offsets/locals recomputed relative to the nearest kept
        // ancestor so dropped mid-chain bones change nothing.
        SkeletonProfile manny;
        manny.id = "unreal-manny";
        manny.name = "Manny Mixamo (UE)";
        manny.hasBind = true;
        std::vector<int> kept;
        for (int i = 0; i < kMannyTrueJoints; ++i) {
            if (keepMannyJoint(kMannyTrueNames[i])) {
                kept.push_back(i);
            }
        }
        // Rest world orientations over the FULL rig (self-consistent).
        std::vector<float> restFlat(static_cast<size_t>(kMannyTrueJoints) * 4);
        std::vector<int> fullParents(kMannyTrueParents, kMannyTrueParents + kMannyTrueJoints);
        std::vector<std::array<float, 3>> fullOffsets;
        for (int i = 0; i < kMannyTrueJoints; ++i) {
            restFlat[i * 4] = kMannyTrueRest[i][0];
            restFlat[i * 4 + 1] = kMannyTrueRest[i][1];
            restFlat[i * 4 + 2] = kMannyTrueRest[i][2];
            restFlat[i * 4 + 3] = kMannyTrueRest[i][3];
            fullOffsets.push_back({kMannyTrueOffsets[i][0], kMannyTrueOffsets[i][1],
                                   kMannyTrueOffsets[i][2]});
        }
        const float origin[3] = {0, 0, 0};
        std::vector<Vector3> restPos;
        std::vector<Quaternion> restWorld;
        Skeleton::forwardKinematicsFull(restFlat.data(), origin, fullParents, fullOffsets,
                                        restPos, restWorld);
        std::map<int, int> newIndex;
        for (size_t k = 0; k < kept.size(); ++k) {
            newIndex[kept[k]] = static_cast<int>(k);
        }
        for (size_t k = 0; k < kept.size(); ++k) {
            const int i = kept[k];
            // Nearest kept ancestor (or -1).
            int anc = kMannyTrueParents[i];
            while (anc >= 0 && newIndex.find(anc) == newIndex.end()) {
                anc = kMannyTrueParents[anc];
            }
            const int newParent = (anc >= 0) ? newIndex[anc] : -1;
            manny.joints.emplace_back(kMannyTrueNames[i]);
            manny.parents.push_back(newParent);
            if (newParent < 0) {
                manny.offsets.push_back(fullOffsets[i]);
                manny.restLocal.push_back({kMannyTrueRest[i][0], kMannyTrueRest[i][1],
                                           kMannyTrueRest[i][2], kMannyTrueRest[i][3]});
            } else {
                const int a = kept[newParent];
                Quaternion qi = QuaternionInvert(restWorld[a]);
                Quaternion lq = QuaternionNormalize(QuaternionMultiply(qi, restWorld[i]));
                Vector3 d = Vector3Subtract(restPos[i], restPos[a]);
                Vector3 off = Vector3RotateByQuaternion(d, qi);
                manny.offsets.push_back({off.x, off.y, off.z});
                manny.restLocal.push_back({lq.x, lq.y, lq.z, lq.w});
            }
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
