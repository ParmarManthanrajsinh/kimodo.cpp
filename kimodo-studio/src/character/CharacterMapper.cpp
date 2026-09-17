#include "character/CharacterMapper.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace studio {
namespace {

struct AliasEntry {
    const char* sourceJoint;
    std::vector<const char*> aliases;
};

const std::vector<AliasEntry>& getStandardAliases() {
    static const std::vector<AliasEntry> table = {
        {"Hips", {"hips", "mixamorig:hips", "skeleton_hips", "pelvis", "bip01_pelvis", "root"}},
        {"Spine1", {"spine1", "spine", "mixamorig:spine", "skeleton_spine", "spine_01", "bip01_spine"}},
        {"Spine2", {"spine2", "mixamorig:spine1", "skeleton_spine1", "spine_02", "bip01_spine1"}},
        {"Chest", {"chest", "spine3", "mixamorig:spine2", "skeleton_spine2", "spine_03", "bip01_spine2"}},
        {"Neck1", {"neck1", "neck", "mixamorig:neck", "skeleton_neck", "neck_01", "bip01_neck"}},
        {"Head", {"head", "mixamorig:head", "skeleton_head", "bip01_head"}},
        {"LeftShoulder", {"leftshoulder", "mixamorig:leftshoulder", "skeleton_clavicle_l", "clavicle_l", "shoulder_l", "bip01_l_clavicle"}},
        {"LeftArm", {"leftarm", "leftupperarm", "mixamorig:leftarm", "skeleton_arm_l", "upperarm_l", "arm_l", "bip01_l_upperarm"}},
        {"LeftForeArm", {"leftforearm", "leftlowerarm", "mixamorig:leftforearm", "skeleton_forearm_l", "forearm_l", "bip01_l_forearm"}},
        {"LeftHand", {"lefthand", "mixamorig:lefthand", "skeleton_hand_l", "hand_l", "bip01_l_hand"}},
        {"RightShoulder", {"rightshoulder", "mixamorig:rightshoulder", "skeleton_clavicle_r", "clavicle_r", "shoulder_r", "bip01_r_clavicle"}},
        {"RightArm", {"rightarm", "rightupperarm", "mixamorig:rightarm", "skeleton_arm_r", "upperarm_r", "arm_r", "bip01_r_upperarm"}},
        {"RightForeArm", {"rightforearm", "rightlowerarm", "mixamorig:rightforearm", "skeleton_forearm_r", "forearm_r", "bip01_r_forearm"}},
        {"RightHand", {"righthand", "mixamorig:righthand", "skeleton_hand_r", "hand_r", "bip01_r_hand"}},
        {"LeftLeg", {"leftleg", "leftupleg", "mixamorig:leftupleg", "skeleton_upleg_l", "thigh_l", "bip01_l_thigh"}},
        {"LeftShin", {"leftshin", "mixamorig:leftleg", "skeleton_leg_l", "calf_l", "bip01_l_calf"}},
        {"LeftFoot", {"leftfoot", "mixamorig:leftfoot", "skeleton_foot_l", "foot_l", "bip01_l_foot"}},
        {"LeftToeBase", {"lefttoebase", "lefttoes", "mixamorig:lefttoebase", "skeleton_toe_l", "ball_l", "bip01_l_toe0"}},
        {"RightLeg", {"rightleg", "rightupleg", "mixamorig:rightupleg", "skeleton_upleg_r", "thigh_r", "bip01_r_thigh"}},
        {"RightShin", {"rightshin", "mixamorig:rightleg", "skeleton_leg_r", "calf_r", "bip01_r_calf"}},
        {"RightFoot", {"rightfoot", "mixamorig:rightfoot", "skeleton_foot_r", "foot_r", "bip01_r_foot"}},
        {"RightToeBase", {"righttoebase", "righttoes", "mixamorig:righttoebase", "skeleton_toe_r", "ball_r", "bip01_r_toe0"}}
    };
    return table;
}

std::string normalizeName(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Replace colon / dots with underscore for consistent matching
    for (char& c : s) {
        if (c == ':' || c == '.' || c == '-') c = '_';
    }
    return s;
}

} // namespace

CharacterBoneMap CharacterMapper::autoMap(const CharacterAsset& asset,
                                         const std::vector<std::string>& sourceJoints) {
    CharacterBoneMap mapping;
    const auto& aliasTable = getStandardAliases();

    // Index available source joints
    std::map<std::string, std::string> normSourceToSource;
    for (const auto& sj : sourceJoints) {
        normSourceToSource[normalizeName(sj)] = sj;
    }

    for (const auto& bone : asset.bones()) {
        const std::string normBone = normalizeName(bone.name);
        bool matched = false;

        // 1. Direct name match
        auto directIt = normSourceToSource.find(normBone);
        if (directIt != normSourceToSource.end()) {
            mapping[bone.name] = directIt->second;
            continue;
        }

        // 2. Alias table lookup
        for (const auto& entry : aliasTable) {
            auto srcIt = normSourceToSource.find(normalizeName(entry.sourceJoint));
            if (srcIt == normSourceToSource.end()) continue;

            for (const char* alias : entry.aliases) {
                if (normBone == normalizeName(alias) ||
                    normBone.find(normalizeName(alias)) != std::string::npos) {
                    mapping[bone.name] = srcIt->second;
                    matched = true;
                    break;
                }
            }
            if (matched) break;
        }

        if (!matched) {
            mapping[bone.name] = "(none)";
        }
    }

    return mapping;
}

bool CharacterMapper::evaluateSkinMatrices(const CharacterAsset& asset,
                                          const Animation& anim,
                                          int frame,
                                          const CharacterBoneMap& mapping,
                                          std::vector<Matrix>& outSkinMatrices) {
    const size_t numBones = asset.bones().size();
    if (numBones == 0 || anim.empty()) {
        return false;
    }

    const int clampedFrame = std::clamp(frame, 0, anim.frames - 1);
    const int numSourceJoints = anim.joints;

    // Index source animation joint names
    std::map<std::string, int> sourceJointIndices;
    for (int j = 0; j < numSourceJoints; ++j) {
        sourceJointIndices[anim.jointNames[j]] = j;
    }

    std::vector<Matrix> localTransforms(numBones, MatrixIdentity());
    std::vector<Matrix> worldTransforms(numBones, MatrixIdentity());

    const float* srcRotBase = anim.localRotationsXyzw.data() + static_cast<size_t>(clampedFrame) * numSourceJoints * 4;
    const float* srcRoot = anim.rootPositions.data() + clampedFrame * 3;

    for (size_t b = 0; b < numBones; ++b) {
        const auto& bone = asset.bones()[b];
        Quaternion localRot = bone.restRotation;
        Vector3 localPos = bone.restPosition;
        Vector3 localScale = bone.restScale;

        auto mapIt = mapping.find(bone.name);
        if (mapIt != mapping.end() && !mapIt->second.empty() && mapIt->second != "(none)") {
            auto srcIt = sourceJointIndices.find(mapIt->second);
            if (srcIt != sourceJointIndices.end()) {
                const int sj = srcIt->second;
                const float* q = srcRotBase + sj * 4;
                localRot = Quaternion{q[0], q[1], q[2], q[3]};
                localRot = QuaternionNormalize(localRot);

                // Apply root translation if this is root/pelvis bone
                if (bone.parent < 0 || sj == 0) {
                    localPos = Vector3{srcRoot[0], srcRoot[1], srcRoot[2]};
                }
            }
        }

        Matrix mScale = MatrixScale(localScale.x, localScale.y, localScale.z);
        Matrix mRot = QuaternionToMatrix(localRot);
        Matrix mTrans = MatrixTranslate(localPos.x, localPos.y, localPos.z);
        localTransforms[b] = MatrixMultiply(MatrixMultiply(mScale, mRot), mTrans);
    }

    // Traverse bone hierarchy to compute world transforms
    for (size_t b = 0; b < numBones; ++b) {
        const int p = asset.bones()[b].parent;
        if (p < 0 || p >= static_cast<int>(numBones)) {
            worldTransforms[b] = localTransforms[b];
        } else {
            worldTransforms[b] = MatrixMultiply(localTransforms[b], worldTransforms[p]);
        }
    }

    // Calculate skin matrices = invBindMatrix * worldTransform
    outSkinMatrices.resize(numBones);
    const auto& ibms = asset.skinningData().inverseBindMatrices;

    for (size_t b = 0; b < numBones; ++b) {
        if (b < ibms.size()) {
            outSkinMatrices[b] = MatrixMultiply(ibms[b], worldTransforms[b]);
        } else {
            outSkinMatrices[b] = worldTransforms[b];
        }
    }

    return true;
}

} // namespace studio
