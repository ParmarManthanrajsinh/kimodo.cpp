#include "character/CharacterMapper.h"
#include "animation/Skeleton.h"
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
        {"Hips", {"hips", "mixamorig:hips", "skeleton_hips", "pelvis", "bip01_pelvis", "root", "skeleton_torso_joint_1", "torso_joint_1"}},
        {"Spine1", {"spine1", "spine", "mixamorig:spine", "skeleton_spine", "spine_01", "bip01_spine", "skeleton_torso_joint_2", "torso_joint_2"}},
        {"Spine2", {"spine2", "mixamorig:spine1", "skeleton_spine1", "spine_02", "bip01_spine1"}},
        {"Chest", {"chest", "spine3", "mixamorig:spine2", "skeleton_spine2", "spine_03", "bip01_spine2", "torso_joint_3", "skeleton_torso_joint_3"}},
        {"Neck1", {"neck1", "neck", "mixamorig:neck", "skeleton_neck", "neck_01", "bip01_neck", "skeleton_neck_joint_1", "neck_joint_1"}},
        {"Head", {"head", "mixamorig:head", "skeleton_head", "bip01_head", "skeleton_neck_joint_2", "neck_joint_2"}},
        {"LeftShoulder", {"leftshoulder", "mixamorig:leftshoulder", "skeleton_clavicle_l", "clavicle_l", "shoulder_l", "bip01_l_clavicle", "skeleton_arm_joint_l__4_", "arm_joint_l_4", "arm_joint_l__4_"}},
        {"LeftArm", {"leftarm", "leftupperarm", "mixamorig:leftarm", "skeleton_arm_l", "upperarm_l", "arm_l", "bip01_l_upperarm", "skeleton_arm_joint_l__3_", "arm_joint_l_3", "arm_joint_l__3_"}},
        {"LeftForeArm", {"leftforearm", "leftlowerarm", "mixamorig:leftforearm", "skeleton_forearm_l", "forearm_l", "bip01_l_forearm", "skeleton_arm_joint_l__2_", "arm_joint_l_2", "arm_joint_l__2_"}},
        {"LeftHand", {"lefthand", "mixamorig:lefthand", "skeleton_hand_l", "hand_l", "bip01_l_hand", "skeleton_arm_joint_l__1_", "arm_joint_l_1", "arm_joint_l__1_"}},
        {"RightShoulder", {"rightshoulder", "mixamorig:rightshoulder", "skeleton_clavicle_r", "clavicle_r", "shoulder_r", "bip01_r_clavicle", "skeleton_arm_joint_r", "arm_joint_r"}},
        {"RightArm", {"rightarm", "rightupperarm", "mixamorig:rightarm", "skeleton_arm_r", "upperarm_r", "arm_r", "bip01_r_upperarm", "skeleton_arm_joint_r__2_", "arm_joint_r_2", "arm_joint_r__2_"}},
        {"RightForeArm", {"rightforearm", "rightlowerarm", "mixamorig:rightforearm", "skeleton_forearm_r", "forearm_r", "bip01_r_forearm", "skeleton_arm_joint_r__3_", "arm_joint_r_3", "arm_joint_r__3_"}},
        {"RightHand", {"righthand", "mixamorig:righthand", "skeleton_hand_r", "hand_r", "bip01_r_hand", "skeleton_arm_joint_r__1_", "arm_joint_r_1", "arm_joint_r__1_"}},
        {"LeftLeg", {"leftleg", "leftupleg", "mixamorig:leftupleg", "skeleton_upleg_l", "thigh_l", "bip01_l_thigh", "leg_joint_l_1", "skeleton_leg_joint_l_1"}},
        {"LeftShin", {"leftshin", "mixamorig:leftleg", "skeleton_leg_l", "calf_l", "bip01_l_calf", "leg_joint_l_2", "skeleton_leg_joint_l_2"}},
        {"LeftFoot", {"leftfoot", "mixamorig:leftfoot", "skeleton_foot_l", "foot_l", "bip01_l_foot", "leg_joint_l_3", "skeleton_leg_joint_l_3"}},
        {"LeftToeBase", {"lefttoebase", "lefttoes", "mixamorig:lefttoebase", "skeleton_toe_l", "ball_l", "bip01_l_toe0", "leg_joint_l_5", "skeleton_leg_joint_l_5"}},
        {"RightLeg", {"rightleg", "rightupleg", "mixamorig:rightupleg", "skeleton_upleg_r", "thigh_r", "bip01_r_thigh", "leg_joint_r_1", "skeleton_leg_joint_r_1"}},
        {"RightShin", {"rightshin", "mixamorig:rightleg", "skeleton_leg_r", "calf_r", "bip01_r_calf", "leg_joint_r_2", "skeleton_leg_joint_r_2"}},
        {"RightFoot", {"rightfoot", "mixamorig:rightfoot", "skeleton_foot_r", "foot_r", "bip01_r_foot", "leg_joint_r_3", "skeleton_leg_joint_r_3"}},
        {"RightToeBase", {"righttoebase", "righttoes", "mixamorig:righttoebase", "skeleton_toe_r", "ball_r", "bip01_r_toe0", "leg_joint_r_5", "skeleton_leg_joint_r_5"}}
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

FCharacterBoneMap FCharacterMapper::autoMap(const FCharacterAsset& asset,
                                         const std::vector<std::string>& sourceJoints) {
    FCharacterBoneMap mapping;
    const auto& aliasTable = getStandardAliases();

    // Index available source joints
    std::map<std::string, std::string> normSourceToSource;
    for (const auto& sj : sourceJoints) {
        normSourceToSource[normalizeName(sj)] = sj;
    }

    for (const auto& bone : asset.GetBones()) {
        const std::string normBone = normalizeName(bone.name);
        bool matched = false;

        // 1. Direct name match
        auto directIt = normSourceToSource.find(normBone);
        if (directIt != normSourceToSource.end()) {
            mapping[bone.name] = directIt->second;
            continue;
        }

        // 2. Exact alias match across all table entries
        for (const auto& entry : aliasTable) {
            auto srcIt = normSourceToSource.find(normalizeName(entry.sourceJoint));
            if (srcIt == normSourceToSource.end()) continue;

            for (const char* alias : entry.aliases) {
                if (normBone == normalizeName(alias)) {
                    mapping[bone.name] = srcIt->second;
                    matched = true;
                    break;
                }
            }
            if (matched) break;
        }
        if (matched) continue;

        // 3. Fallback: Substring alias match (only if no exact match exists)
        for (const auto& entry : aliasTable) {
            auto srcIt = normSourceToSource.find(normalizeName(entry.sourceJoint));
            if (srcIt == normSourceToSource.end()) continue;

            for (const char* alias : entry.aliases) {
                std::string normAlias = normalizeName(alias);
                if (normBone.find(normAlias) != std::string::npos) {
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

bool FCharacterMapper::evaluateSkinMatrices(const FCharacterAsset& asset,
                                          const FAnimation& anim,
                                          int frame,
                                          const FCharacterBoneMap& mapping,
                                          std::vector<Matrix>& outSkinMatrices,
                                          std::vector<Vector3>* outBonePositions) {
    const size_t numBones = asset.GetBones().size();
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

    // 1. Evaluate SOMA world positions & orientations for animated frame
    const float* srcRot = anim.localRotationsXyzw.data() + static_cast<size_t>(clampedFrame) * numSourceJoints * 4;
    const float* srcRoot = anim.rootPositions.data() + clampedFrame * 3;
    std::vector<Vector3> somaWorldPos;
    std::vector<Quaternion> somaWorldRot;
    FSkeleton::ForwardKinematicsFull(srcRot, srcRoot, anim.parents, anim.offsets, somaWorldPos, somaWorldRot);

    // 2. Evaluate SOMA rest pose world positions & orientations (identity rotations)
    std::vector<float> somaRestRot(numSourceJoints * 4, 0.0f);
    for (int j = 0; j < numSourceJoints; ++j) somaRestRot[j * 4 + 3] = 1.0f;
    float somaRestRoot[3] = {0.0f, 0.95f, 0.0f};
    if (!anim.rootPositions.empty()) {
        somaRestRoot[0] = anim.rootPositions[0];
        somaRestRoot[1] = anim.rootPositions[1];
        somaRestRoot[2] = anim.rootPositions[2];
    }
    std::vector<Vector3> somaRestWorldPos;
    std::vector<Quaternion> somaRestWorldRot;
    FSkeleton::ForwardKinematicsFull(somaRestRot.data(), somaRestRoot, anim.parents, anim.offsets, somaRestWorldPos, somaRestWorldRot);

    // 3. Extract character rest world transforms
    std::vector<Vector3> targetRestWorldPos(numBones);
    std::vector<Quaternion> targetRestWorldRot(numBones);
    for (size_t b = 0; b < numBones; ++b) {
        const Matrix& rw = asset.GetBones()[b].worldTransform;
        targetRestWorldPos[b] = Vector3{rw.m12, rw.m13, rw.m14};
        targetRestWorldRot[b] = QuaternionNormalize(QuaternionFromMatrix(rw));
    }

    // 4. Compute target bone animated world orientations & positions
    std::vector<Quaternion> targetAnimWorldRot(numBones, Quaternion{0.0f, 0.0f, 0.0f, 1.0f});
    std::vector<Vector3> targetAnimWorldPos(numBones, Vector3{0.0f, 0.0f, 0.0f});
    std::vector<Matrix> worldTransforms(numBones, MatrixIdentity());

    Vector3 somaRootDelta = Vector3Subtract(
        somaWorldPos.empty() ? Vector3{0.0f, 0.0f, 0.0f} : somaWorldPos[0],
        somaRestWorldPos.empty() ? Vector3{0.0f, 0.0f, 0.0f} : somaRestWorldPos[0]
    );

    // Proportional root delta scaling (matches character height to SOMA height so feet remain grounded)
    float somaHipsHeight = (somaRestWorldPos.empty() || somaRestWorldPos[0].y <= 0.01f) ? 0.95f : somaRestWorldPos[0].y;
    float charHipsHeight = targetRestWorldPos.empty() ? 1.0f : targetRestWorldPos[0].y;
    float rootScale = (somaHipsHeight > 0.01f && charHipsHeight > 0.01f) ? (charHipsHeight / somaHipsHeight) : 1.0f;
    somaRootDelta = Vector3Scale(somaRootDelta, rootScale);

    for (size_t b = 0; b < numBones; ++b) {
        const auto& bone = asset.GetBones()[b];
        const int p = bone.parent;

        // Determine orientation
        bool mapped = false;
        int sj = -1;
        auto mapIt = mapping.find(bone.name);
        if (mapIt != mapping.end() && !mapIt->second.empty() && mapIt->second != "(none)") {
            auto srcIt = sourceJointIndices.find(mapIt->second);
            if (srcIt != sourceJointIndices.end()) {
                sj = srcIt->second;
                if (sj >= 0 && sj < static_cast<int>(somaWorldRot.size())) {
                    mapped = true;
                }
            }
        }

        if (mapped) {
            // Delta world rotation of SOMA joint from its rest pose
            Quaternion somaDeltaRot = QuaternionMultiply(somaWorldRot[sj], QuaternionInvert(somaRestWorldRot[sj]));
            targetAnimWorldRot[b] = QuaternionNormalize(QuaternionMultiply(somaDeltaRot, targetRestWorldRot[b]));
        } else if (p >= 0 && p < static_cast<int>(numBones)) {
            // Follow parent's delta rotation
            Quaternion parentDeltaRot = QuaternionMultiply(targetAnimWorldRot[p], QuaternionInvert(targetRestWorldRot[p]));
            targetAnimWorldRot[b] = QuaternionNormalize(QuaternionMultiply(parentDeltaRot, targetRestWorldRot[b]));
        } else {
            targetAnimWorldRot[b] = targetRestWorldRot[b];
        }

        // Determine position with bone length preservation
        if (p < 0) {
            // Root position: character rest position + SOMA root translation delta
            targetAnimWorldPos[b] = Vector3Add(targetRestWorldPos[b], somaRootDelta);
        } else {
            // Child position: rotate rest bone offset vector by animated parent orientation
            Vector3 restOffsetWorld = Vector3Subtract(targetRestWorldPos[b], targetRestWorldPos[p]);
            Vector3 restOffsetLocal = Vector3RotateByQuaternion(restOffsetWorld, QuaternionInvert(targetRestWorldRot[p]));
            Vector3 animOffsetWorld = Vector3RotateByQuaternion(restOffsetLocal, targetAnimWorldRot[p]);
            targetAnimWorldPos[b] = Vector3Add(targetAnimWorldPos[p], animOffsetWorld);
        }

        // Construct world transform matrix
        Matrix mScale = MatrixScale(bone.restScale.x, bone.restScale.y, bone.restScale.z);
        Matrix mRot = QuaternionToMatrix(targetAnimWorldRot[b]);
        Matrix mTrans = MatrixTranslate(targetAnimWorldPos[b].x, targetAnimWorldPos[b].y, targetAnimWorldPos[b].z);
        worldTransforms[b] = MatrixMultiply(MatrixMultiply(mScale, mRot), mTrans);
    }

    // 5. Calculate final skin matrices = invBindMatrix * worldTransform
    outSkinMatrices.resize(numBones);
    const auto& ibms = asset.GetSkinningData().inverseBindMatrices;

    for (size_t b = 0; b < numBones; ++b) {
        if (b < ibms.size()) {
            outSkinMatrices[b] = MatrixMultiply(ibms[b], worldTransforms[b]);
        } else {
            outSkinMatrices[b] = worldTransforms[b];
        }
    }

    if (outBonePositions) {
        *outBonePositions = std::move(targetAnimWorldPos);
    }

    return true;
}

} // namespace studio
