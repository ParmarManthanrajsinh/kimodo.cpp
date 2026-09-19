#include "animation/SomaPresentation.h"
#include "animation/Skeleton.h"
#include "raymath.h"

#include <cmath>
#include <map>

namespace studio {

namespace {

const std::vector<std::string>& get_presentation_names() {
    static const std::vector<std::string> names = {
        // Core 30 SOMA joints
        "Hips", "Spine1", "Spine2", "Chest", "Neck1", "Neck2", "Head", "Jaw", "LeftEye", "RightEye", "LeftShoulder",
        "LeftArm", "LeftForeArm", "LeftHand", "LeftHandThumbEnd", "LeftHandMiddleEnd", "RightShoulder", "RightArm",
        "RightForeArm", "RightHand", "RightHandThumbEnd", "RightHandMiddleEnd", "LeftLeg", "LeftShin", "LeftFoot",
        "LeftToeBase", "RightLeg", "RightShin", "RightFoot", "RightToeBase",
        // Extended fingers (Left)
        "LeftHandThumb1", "LeftHandThumb2", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandMiddle1",
        "LeftHandMiddle2", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3", "LeftHandPinky1", "LeftHandPinky2",
        "LeftHandPinky3",
        // Extended fingers (Right)
        "RightHandThumb1", "RightHandThumb2", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
        "RightHandMiddle1", "RightHandMiddle2", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandPinky1",
        "RightHandPinky2", "RightHandPinky3",
        // Extended toe ends
        "LeftToeEnd", "RightToeEnd"};
    return names;
}

const std::vector<int>& get_presentation_parents() {
    static const std::vector<int> parents = [] {
        std::vector<int> p = {// Core 30 parents
                              -1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 3, 10, 11, 12, 13, 13, 3, 16, 17, 18, 19, 19, 0, 22, 23,
                              24, 0, 26, 27, 28,
                              // Left fingers parented to LeftHand (13) and chains
                              13, 30, 13, 32, 33, 13, 35, 13, 37, 38, 13, 40, 41,
                              // Right fingers parented to RightHand (19) and chains
                              19, 43, 19, 45, 46, 19, 48, 19, 50, 51, 19, 53, 54,
                              // Toe ends parented to ToeBase (25 and 29)
                              25, 29};
        return p;
    }();
    return parents;
}

const std::vector<std::array<float, 3>>& get_presentation_offsets() {
    static const std::vector<std::array<float, 3>> offsets = [] {
        std::vector<std::array<float, 3>> offs(get_presentation_names().size(), {0, 0, 0});
        // Core 30 offsets from Soma30Spec
        for (int i = 0; i < kSomaJoints; ++i) {
            offs[i] = Soma30Spec::offsets[i];
        }
        // Extended finger default rest offsets (relative to parents)
        // Left fingers (pointing +X / +Z)
        offs[30] = {0.035f, -0.015f, 0.025f};  // Thumb1
        offs[31] = {0.030f, -0.005f, 0.015f};  // Thumb2
        offs[32] = {0.075f, 0.005f, 0.015f};   // Index1
        offs[33] = {0.040f, 0.000f, 0.002f};   // Index2
        offs[34] = {0.030f, 0.000f, 0.000f};   // Index3
        offs[35] = {0.080f, 0.000f, 0.000f};   // Middle1
        offs[36] = {0.045f, 0.000f, 0.000f};   // Middle2
        offs[37] = {0.075f, -0.005f, -0.012f}; // Ring1
        offs[38] = {0.038f, 0.000f, 0.000f};   // Ring2
        offs[39] = {0.028f, 0.000f, 0.000f};   // Ring3
        offs[40] = {0.065f, -0.010f, -0.022f}; // Pinky1
        offs[41] = {0.032f, 0.000f, 0.000f};   // Pinky2
        offs[42] = {0.024f, 0.000f, 0.000f};   // Pinky3

        // Right fingers (pointing -X / +Z)
        offs[43] = {-0.035f, -0.015f, 0.025f};  // Thumb1
        offs[44] = {-0.030f, -0.005f, 0.015f};  // Thumb2
        offs[45] = {-0.075f, 0.005f, 0.015f};   // Index1
        offs[46] = {-0.040f, 0.000f, 0.002f};   // Index2
        offs[47] = {-0.030f, 0.000f, 0.000f};   // Index3
        offs[48] = {-0.080f, 0.000f, 0.000f};   // Middle1
        offs[49] = {-0.045f, 0.000f, 0.000f};   // Middle2
        offs[50] = {-0.075f, -0.005f, -0.012f}; // Ring1
        offs[51] = {-0.038f, 0.000f, 0.000f};   // Ring2
        offs[52] = {-0.028f, 0.000f, 0.000f};   // Ring3
        offs[53] = {-0.065f, -0.010f, -0.022f}; // Pinky1
        offs[54] = {-0.032f, 0.000f, 0.000f};   // Pinky2
        offs[55] = {-0.024f, 0.000f, 0.000f};   // Pinky3

        // Toe ends
        offs[56] = {0.0f, -0.010f, 0.070f}; // LeftToeEnd
        offs[57] = {0.0f, -0.010f, 0.070f}; // RightToeEnd
        return offs;
    }();
    return offsets;
}

} // namespace

const std::vector<std::string>& SomaPresentationSpec::joint_names() { return get_presentation_names(); }

const std::vector<int>& SomaPresentationSpec::parents() { return get_presentation_parents(); }

const std::vector<std::array<float, 3>>& SomaPresentationSpec::default_offsets() { return get_presentation_offsets(); }

int SomaPresentationSpec::joint_index(const std::string& name) {
    const auto& names = get_presentation_names();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name)
            return static_cast<int>(i);
    }
    return -1;
}

bool SomaPresentation::ExpandSoma30(const Animation& in, Animation& out, std::string& error) {
    if (in.empty() || in.joints < kSomaJoints) {
        error = "Invalid SOMA30 source animation";
        return false;
    }

    const auto& p_names = get_presentation_names();
    const auto& p_parents = get_presentation_parents();
    const auto& p_offsets = get_presentation_offsets();
    const int target_joints = static_cast<int>(p_names.size());

    out.frames = in.frames;
    out.fps = in.fps;
    out.joints = target_joints;
    out.skeleton_name = "soma-presentation";
    out.joint_names = p_names;
    out.parents = p_parents;
    out.offsets = p_offsets;
    out.root_positions = in.root_positions;
    out.local_rotations_xyzw.assign(static_cast<size_t>(in.frames) * target_joints * 4, 0.0f);

    // Map source SOMA joints and set default identity for extended joints
    for (int f = 0; f < in.frames; ++f) {
        const float* src_frame = in.local_rotations_xyzw.data() + static_cast<size_t>(f) * in.joints * 4;
        float* dst_frame = out.local_rotations_xyzw.data() + static_cast<size_t>(f) * target_joints * 4;

        // Copy SOMA30 rotations
        for (int j = 0; j < kSomaJoints; ++j) {
            dst_frame[j * 4 + 0] = src_frame[j * 4 + 0];
            dst_frame[j * 4 + 1] = src_frame[j * 4 + 1];
            dst_frame[j * 4 + 2] = src_frame[j * 4 + 2];
            dst_frame[j * 4 + 3] = src_frame[j * 4 + 3];
        }

        // Initialize extended finger/toe joints to normalized identity {0, 0, 0, 1}
        for (int j = kSomaJoints; j < target_joints; ++j) {
            dst_frame[j * 4 + 0] = 0.0f;
            dst_frame[j * 4 + 1] = 0.0f;
            dst_frame[j * 4 + 2] = 0.0f;
            dst_frame[j * 4 + 3] = 1.0f;
        }
    }

    return true;
}

SomaPresentation::ValidationResult SomaPresentation::Validate(const Animation& anim) {
    ValidationResult res;
    if (anim.empty()) {
        res.valid = false;
        res.errors.push_back("Animation is empty");
        return res;
    }

    const int J = anim.joints;
    if (static_cast<int>(anim.joint_names.size()) != J || static_cast<int>(anim.parents.size()) != J ||
        static_cast<int>(anim.offsets.size()) != J) {
        res.valid = false;
        res.hierarchy_valid = false;
        res.errors.push_back("Hierarchy dimension mismatch");
    }

    // Check root and parents
    int root_count = 0;
    for (int j = 0; j < J; ++j) {
        const int p = anim.parents[j];
        if (p < 0) {
            root_count++;
        } else if (p >= j) {
            res.valid = false;
            res.hierarchy_valid = false;
            res.errors.push_back("Parent index not strictly less than child index at joint " + std::to_string(j));
        }
    }
    if (root_count != 1) {
        res.warnings.push_back("Expected exactly 1 root joint, found " + std::to_string(root_count));
    }

    // Check finite numbers
    for (size_t i = 0; i < anim.local_rotations_xyzw.size(); ++i) {
        if (!std::isfinite(anim.local_rotations_xyzw[i])) {
            res.valid = false;
            res.is_finite = false;
            res.errors.push_back("Non-finite rotation value found");
            break;
        }
    }
    for (size_t i = 0; i < anim.root_positions.size(); ++i) {
        if (!std::isfinite(anim.root_positions[i])) {
            res.valid = false;
            res.is_finite = false;
            res.errors.push_back("Non-finite root position value found");
            break;
        }
    }

    // Left/Right symmetry check
    std::map<std::string, int> name_map;
    for (int j = 0; j < J; ++j) {
        name_map[anim.joint_names[j]] = j;
    }
    for (const auto& name : anim.joint_names) {
        if (name.rfind("Left", 0) == 0) {
            std::string right_name = "Right" + name.substr(4);
            if (name_map.find(right_name) == name_map.end()) {
                res.leftRightConsistent = false;
                res.warnings.push_back("Missing mirrored right joint for: " + name);
            }
        }
    }

    // Rest pose forward kinematics standing check
    if (res.hierarchy_valid && res.is_finite && J > 0) {
        std::vector<float> ident_rots(static_cast<size_t>(J) * 4, 0.0f);
        for (int j = 0; j < J; ++j)
            ident_rots[j * 4 + 3] = 1.0f;
        const float origin[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> world_pos;
        Skeleton::ForwardKinematicsGeneral(ident_rots.data(), origin, anim.parents, anim.offsets, world_pos);

        int head_idx = -1, hips_idx = -1, foot_idx = -1;
        for (int j = 0; j < J; ++j) {
            const std::string& n = anim.joint_names[j];
            if (n == "Head" || n == "head")
                head_idx = j;
            if (n == "Hips" || n == "hips" || n == "pelvis")
                hips_idx = j;
            if (n == "LeftFoot" || n == "foot_l" || n == "LeftFoot_End")
                foot_idx = j;
        }

        if (head_idx >= 0 && hips_idx >= 0 && foot_idx >= 0) {
            res.torso_span = world_pos[head_idx].y - world_pos[hips_idx].y;
            res.standing_height = world_pos[head_idx].y - world_pos[foot_idx].y;
            if (res.torso_span < 0.2f || res.standing_height < 0.8f) {
                res.standing_rest_pose = false;
                res.warnings.push_back(
                    "Rest pose collapsed or non-standing: height=" + std::to_string(res.standing_height) + "m");
            }
        }
    }

    return res;
}

} // namespace studio
