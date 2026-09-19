#include "character/CharacterMapper.h"
#include "animation/Skeleton.h"
#include "raymath.h"

#include <algorithm>

namespace studio {
namespace {

struct AliasEntry {
    const char* source_joint;
    std::vector<const char*> aliases;
};

const std::vector<AliasEntry>& getStandardAliases() {
    static const std::vector<AliasEntry> table = {
        {"Hips",
         {"hips", "mixamorig:hips", "skeleton_hips", "pelvis", "bip01_pelvis", "root", "skeleton_torso_joint_1",
          "torso_joint_1"}},
        {"Spine1",
         {"spine1", "spine", "mixamorig:spine", "skeleton_spine", "spine_01", "bip01_spine", "skeleton_torso_joint_2",
          "torso_joint_2"}},
        {"Spine2", {"spine2", "mixamorig:spine1", "skeleton_spine1", "spine_02", "bip01_spine1"}},
        {"Chest",
         {"chest", "spine3", "mixamorig:spine2", "skeleton_spine2", "spine_03", "bip01_spine2", "torso_joint_3",
          "skeleton_torso_joint_3"}},
        {"Neck1",
         {"neck1", "neck", "mixamorig:neck", "skeleton_neck", "neck_01", "bip01_neck", "skeleton_neck_joint_1",
          "neck_joint_1"}},
        {"Head", {"head", "mixamorig:head", "skeleton_head", "bip01_head", "skeleton_neck_joint_2", "neck_joint_2"}},
        {"LeftShoulder",
         {"leftshoulder", "mixamorig:leftshoulder", "skeleton_clavicle_l", "clavicle_l", "shoulder_l",
          "bip01_l_clavicle", "skeleton_arm_joint_l__4_", "arm_joint_l_4", "arm_joint_l__4_"}},
        {"LeftArm",
         {"leftarm", "leftupperarm", "mixamorig:leftarm", "skeleton_arm_l", "upperarm_l", "arm_l", "bip01_l_upperarm",
          "skeleton_arm_joint_l__3_", "arm_joint_l_3", "arm_joint_l__3_"}},
        {"LeftForeArm",
         {"leftforearm", "leftlowerarm", "mixamorig:leftforearm", "skeleton_forearm_l", "forearm_l", "bip01_l_forearm",
          "skeleton_arm_joint_l__2_", "arm_joint_l_2", "arm_joint_l__2_"}},
        {"LeftHand",
         {"lefthand", "mixamorig:lefthand", "skeleton_hand_l", "hand_l", "bip01_l_hand", "skeleton_arm_joint_l__1_",
          "arm_joint_l_1", "arm_joint_l__1_"}},
        {"RightShoulder",
         {"rightshoulder", "mixamorig:rightshoulder", "skeleton_clavicle_r", "clavicle_r", "shoulder_r",
          "bip01_r_clavicle", "skeleton_arm_joint_r", "arm_joint_r"}},
        {"RightArm",
         {"rightarm", "rightupperarm", "mixamorig:rightarm", "skeleton_arm_r", "upperarm_r", "arm_r",
          "bip01_r_upperarm", "skeleton_arm_joint_r__2_", "arm_joint_r_2", "arm_joint_r__2_"}},
        {"RightForeArm",
         {"rightforearm", "rightlowerarm", "mixamorig:rightforearm", "skeleton_forearm_r", "forearm_r",
          "bip01_r_forearm", "skeleton_arm_joint_r__3_", "arm_joint_r_3", "arm_joint_r__3_"}},
        {"RightHand",
         {"righthand", "mixamorig:righthand", "skeleton_hand_r", "hand_r", "bip01_r_hand", "skeleton_arm_joint_r__1_",
          "arm_joint_r_1", "arm_joint_r__1_"}},
        {"LeftLeg",
         {"leftleg", "leftupleg", "mixamorig:leftupleg", "skeleton_upleg_l", "thigh_l", "bip01_l_thigh",
          "leg_joint_l_1", "skeleton_leg_joint_l_1"}},
        {"LeftShin",
         {"leftshin", "mixamorig:leftleg", "skeleton_leg_l", "calf_l", "bip01_l_calf", "leg_joint_l_2",
          "skeleton_leg_joint_l_2"}},
        {"LeftFoot",
         {"leftfoot", "mixamorig:leftfoot", "skeleton_foot_l", "foot_l", "bip01_l_foot", "leg_joint_l_3",
          "skeleton_leg_joint_l_3"}},
        {"LeftToeBase",
         {"lefttoebase", "lefttoes", "mixamorig:lefttoebase", "skeleton_toe_l", "ball_l", "bip01_l_toe0",
          "leg_joint_l_5", "skeleton_leg_joint_l_5"}},
        {"RightLeg",
         {"rightleg", "rightupleg", "mixamorig:rightupleg", "skeleton_upleg_r", "thigh_r", "bip01_r_thigh",
          "leg_joint_r_1", "skeleton_leg_joint_r_1"}},
        {"RightShin",
         {"rightshin", "mixamorig:rightleg", "skeleton_leg_r", "calf_r", "bip01_r_calf", "leg_joint_r_2",
          "skeleton_leg_joint_r_2"}},
        {"RightFoot",
         {"rightfoot", "mixamorig:rightfoot", "skeleton_foot_r", "foot_r", "bip01_r_foot", "leg_joint_r_3",
          "skeleton_leg_joint_r_3"}},
        {"RightToeBase",
         {"righttoebase", "righttoes", "mixamorig:righttoebase", "skeleton_toe_r", "ball_r", "bip01_r_toe0",
          "leg_joint_r_5", "skeleton_leg_joint_r_5"}}};
    return table;
}

std::string normalize_name(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Replace colon / dots with underscore for consistent matching
    for (char& c : s) {
        if (c == ':' || c == '.' || c == '-')
            c = '_';
    }
    return s;
}

} // namespace

CharacterBoneMap CharacterMapper::AutoMap(const CharacterAsset& asset, const std::vector<std::string>& source_joints) {
    CharacterBoneMap mapping;
    const auto& alias_table = getStandardAliases();

    // Index available source joints
    std::map<std::string, std::string> norm_source_to_source;
    for (const auto& sj : source_joints) {
        norm_source_to_source[normalize_name(sj)] = sj;
    }

    for (const auto& bone : asset.GetBones()) {
        const std::string norm_bone = normalize_name(bone.name);
        bool matched = false;

        // 1. Direct name match
        auto direct_it = norm_source_to_source.find(norm_bone);
        if (direct_it != norm_source_to_source.end()) {
            mapping[bone.name] = direct_it->second;
            continue;
        }

        // 2. Exact alias match across all table entries
        for (const auto& entry : alias_table) {
            auto src_it = norm_source_to_source.find(normalize_name(entry.source_joint));
            if (src_it == norm_source_to_source.end())
                continue;

            for (const char* alias : entry.aliases) {
                if (norm_bone == normalize_name(alias)) {
                    mapping[bone.name] = src_it->second;
                    matched = true;
                    break;
                }
            }
            if (matched)
                break;
        }
        if (matched)
            continue;

        // 3. Fallback: Substring alias match (only if no exact match exists)
        for (const auto& entry : alias_table) {
            auto src_it = norm_source_to_source.find(normalize_name(entry.source_joint));
            if (src_it == norm_source_to_source.end())
                continue;

            for (const char* alias : entry.aliases) {
                std::string norm_alias = normalize_name(alias);
                if (norm_bone.find(norm_alias) != std::string::npos) {
                    mapping[bone.name] = src_it->second;
                    matched = true;
                    break;
                }
            }
            if (matched)
                break;
        }

        if (!matched) {
            mapping[bone.name] = "(none)";
        }
    }

    return mapping;
}

bool CharacterMapper::EvaluateSkinMatrices(const CharacterAsset& asset, const Animation& anim, int frame,
                                           const CharacterBoneMap& mapping, std::vector<Matrix>& out_skin_matrices,
                                           std::vector<Vector3>* out_bone_positions) {
    const size_t num_bones = asset.GetBones().size();
    if (num_bones == 0 || anim.empty()) {
        return false;
    }

    const int clamped_frame = std::clamp(frame, 0, anim.frames - 1);
    const int num_source_joints = anim.joints;

    // Index source animation joint names
    std::map<std::string, int> source_joint_indices;
    for (int j = 0; j < num_source_joints; ++j) {
        source_joint_indices[anim.joint_names[j]] = j;
    }

    // 1. Evaluate SOMA world positions & orientations for animated frame
    const float* src_rot =
        anim.local_rotations_xyzw.data() + static_cast<size_t>(clamped_frame) * num_source_joints * 4;
    const float* src_root = anim.root_positions.data() + clamped_frame * 3;
    std::vector<Vector3> soma_world_pos;
    std::vector<Quaternion> soma_world_rot;
    Skeleton::ForwardKinematicsFull(src_rot, src_root, anim.parents, anim.offsets, soma_world_pos, soma_world_rot);

    // 2. Evaluate SOMA rest pose world positions & orientations (identity rotations)
    std::vector<float> soma_rest_rot(num_source_joints * 4, 0.0f);
    for (int j = 0; j < num_source_joints; ++j)
        soma_rest_rot[j * 4 + 3] = 1.0f;
    float soma_rest_root[3] = {0.0f, 0.95f, 0.0f};
    if (!anim.root_positions.empty()) {
        soma_rest_root[0] = anim.root_positions[0];
        soma_rest_root[1] = anim.root_positions[1];
        soma_rest_root[2] = anim.root_positions[2];
    }
    std::vector<Vector3> soma_rest_world_pos;
    std::vector<Quaternion> soma_rest_world_rot;
    Skeleton::ForwardKinematicsFull(soma_rest_rot.data(), soma_rest_root, anim.parents, anim.offsets,
                                    soma_rest_world_pos, soma_rest_world_rot);

    // 3. Extract character rest world transforms
    std::vector<Vector3> target_rest_world_pos(num_bones);
    std::vector<Quaternion> target_rest_world_rot(num_bones);
    for (size_t b = 0; b < num_bones; ++b) {
        const Matrix& rw = asset.GetBones()[b].world_transform;
        target_rest_world_pos[b] = Vector3{rw.m12, rw.m13, rw.m14};
        target_rest_world_rot[b] = QuaternionNormalize(QuaternionFromMatrix(rw));
    }

    // 4. Compute target bone animated world orientations & positions
    std::vector<Quaternion> target_anim_world_rot(num_bones, Quaternion{0.0f, 0.0f, 0.0f, 1.0f});
    std::vector<Vector3> target_anim_world_pos(num_bones, Vector3{0.0f, 0.0f, 0.0f});
    std::vector<Matrix> world_transforms(num_bones, MatrixIdentity());

    Vector3 soma_root_delta =
        Vector3Subtract(soma_world_pos.empty() ? Vector3{0.0f, 0.0f, 0.0f} : soma_world_pos[0],
                        soma_rest_world_pos.empty() ? Vector3{0.0f, 0.0f, 0.0f} : soma_rest_world_pos[0]);

    // Proportional root delta scaling (matches character height to SOMA height so feet remain grounded)
    float soma_hips_height =
        (soma_rest_world_pos.empty() || soma_rest_world_pos[0].y <= 0.01f) ? 0.95f : soma_rest_world_pos[0].y;
    float char_hips_height = target_rest_world_pos.empty() ? 1.0f : target_rest_world_pos[0].y;
    float root_scale =
        (soma_hips_height > 0.01f && char_hips_height > 0.01f) ? (char_hips_height / soma_hips_height) : 1.0f;
    soma_root_delta = Vector3Scale(soma_root_delta, root_scale);

    for (size_t b = 0; b < num_bones; ++b) {
        const auto& bone = asset.GetBones()[b];
        const int p = bone.parent;

        // Determine orientation
        bool mapped = false;
        int sj = -1;
        auto map_it = mapping.find(bone.name);
        if (map_it != mapping.end() && !map_it->second.empty() && map_it->second != "(none)") {
            auto src_it = source_joint_indices.find(map_it->second);
            if (src_it != source_joint_indices.end()) {
                sj = src_it->second;
                if (sj >= 0 && sj < static_cast<int>(soma_world_rot.size())) {
                    mapped = true;
                }
            }
        }

        if (mapped) {
            // Delta world rotation of SOMA joint from its rest pose
            Quaternion soma_delta_rot =
                QuaternionMultiply(soma_world_rot[sj], QuaternionInvert(soma_rest_world_rot[sj]));
            target_anim_world_rot[b] =
                QuaternionNormalize(QuaternionMultiply(soma_delta_rot, target_rest_world_rot[b]));
        } else if (p >= 0 && p < static_cast<int>(num_bones)) {
            // Follow parent's delta rotation
            Quaternion parentDeltaRot =
                QuaternionMultiply(target_anim_world_rot[p], QuaternionInvert(target_rest_world_rot[p]));
            target_anim_world_rot[b] =
                QuaternionNormalize(QuaternionMultiply(parentDeltaRot, target_rest_world_rot[b]));
        } else {
            target_anim_world_rot[b] = target_rest_world_rot[b];
        }

        // Determine position with bone length preservation
        if (p < 0) {
            // Root position: character rest position + SOMA root translation delta
            target_anim_world_pos[b] = Vector3Add(target_rest_world_pos[b], soma_root_delta);
        } else {
            // Child position: rotate rest bone offset vector by animated parent orientation
            Vector3 rest_offset_world = Vector3Subtract(target_rest_world_pos[b], target_rest_world_pos[p]);
            Vector3 rest_offset_local =
                Vector3RotateByQuaternion(rest_offset_world, QuaternionInvert(target_rest_world_rot[p]));
            Vector3 anim_offset_world = Vector3RotateByQuaternion(rest_offset_local, target_anim_world_rot[p]);
            target_anim_world_pos[b] = Vector3Add(target_anim_world_pos[p], anim_offset_world);
        }

        // Construct world transform matrix
        Matrix m_scale = MatrixScale(bone.rest_scale.x, bone.rest_scale.y, bone.rest_scale.z);
        Matrix m_rot = QuaternionToMatrix(target_anim_world_rot[b]);
        Matrix m_trans =
            MatrixTranslate(target_anim_world_pos[b].x, target_anim_world_pos[b].y, target_anim_world_pos[b].z);
        world_transforms[b] = MatrixMultiply(MatrixMultiply(m_scale, m_rot), m_trans);
    }

    // 5. Calculate final skin matrices = invBindMatrix * worldTransform
    out_skin_matrices.resize(num_bones);
    const auto& ibms = asset.GetSkinningData().inverse_bind_matrices;

    for (size_t b = 0; b < num_bones; ++b) {
        if (b < ibms.size()) {
            out_skin_matrices[b] = MatrixMultiply(ibms[b], world_transforms[b]);
        } else {
            out_skin_matrices[b] = world_transforms[b];
        }
    }

    if (out_bone_positions) {
        *out_bone_positions = std::move(target_anim_world_pos);
    }

    return true;
}

} // namespace studio
