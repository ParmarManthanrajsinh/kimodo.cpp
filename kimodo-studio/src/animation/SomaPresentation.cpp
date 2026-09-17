#include "animation/SomaPresentation.h"
#include "animation/Skeleton.h"
#include "raymath.h"

#include <cmath>
#include <map>

namespace studio {

namespace {

const std::vector<std::string>& getPresentationNames() {
    static const std::vector<std::string> names = {
        // Core 30 SOMA joints
        "Hips", "Spine1", "Spine2", "Chest", "Neck1", "Neck2", "Head", "Jaw",
        "LeftEye", "RightEye", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
        "LeftHandThumbEnd", "LeftHandMiddleEnd", "RightShoulder", "RightArm", "RightForeArm",
        "RightHand", "RightHandThumbEnd", "RightHandMiddleEnd", "LeftLeg", "LeftShin", "LeftFoot",
        "LeftToeBase", "RightLeg", "RightShin", "RightFoot", "RightToeBase",
        // Extended fingers (Left)
        "LeftHandThumb1", "LeftHandThumb2", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
        "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3",
        "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3",
        // Extended fingers (Right)
        "RightHandThumb1", "RightHandThumb2", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
        "RightHandMiddle1", "RightHandMiddle2", "RightHandRing1", "RightHandRing2", "RightHandRing3",
        "RightHandPinky1", "RightHandPinky2", "RightHandPinky3",
        // Extended toe ends
        "LeftToeEnd", "RightToeEnd"
    };
    return names;
}

const std::vector<int>& getPresentationParents() {
    static const std::vector<int> parents = [] {
        std::vector<int> p = {
            // Core 30 parents
            -1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 3, 10, 11, 12, 13, 13, 3, 16, 17, 18, 19, 19,
            0, 22, 23, 24, 0, 26, 27, 28,
            // Left fingers parented to LeftHand (13) and chains
            13, 30, 13, 32, 33, 13, 35, 13, 37, 38, 13, 40, 41,
            // Right fingers parented to RightHand (19) and chains
            19, 43, 19, 45, 46, 19, 48, 19, 50, 51, 19, 53, 54,
            // Toe ends parented to ToeBase (25 and 29)
            25, 29
        };
        return p;
    }();
    return parents;
}

const std::vector<std::array<float, 3>>& getPresentationOffsets() {
    static const std::vector<std::array<float, 3>> offsets = [] {
        std::vector<std::array<float, 3>> offs(getPresentationNames().size(), {0, 0, 0});
        // Core 30 offsets from Soma30Spec
        for (int i = 0; i < kSomaJoints; ++i) {
            offs[i] = Soma30Spec::offsets[i];
        }
        // Extended finger default rest offsets (relative to parents)
        // Left fingers (pointing +X / +Z)
        offs[30] = {0.035f, -0.015f, 0.025f}; // Thumb1
        offs[31] = {0.030f, -0.005f, 0.015f}; // Thumb2
        offs[32] = {0.075f, 0.005f, 0.015f};  // Index1
        offs[33] = {0.040f, 0.000f, 0.002f};  // Index2
        offs[34] = {0.030f, 0.000f, 0.000f};  // Index3
        offs[35] = {0.080f, 0.000f, 0.000f};  // Middle1
        offs[36] = {0.045f, 0.000f, 0.000f};  // Middle2
        offs[37] = {0.075f, -0.005f, -0.012f};// Ring1
        offs[38] = {0.038f, 0.000f, 0.000f};  // Ring2
        offs[39] = {0.028f, 0.000f, 0.000f};  // Ring3
        offs[40] = {0.065f, -0.010f, -0.022f};// Pinky1
        offs[41] = {0.032f, 0.000f, 0.000f};  // Pinky2
        offs[42] = {0.024f, 0.000f, 0.000f};  // Pinky3

        // Right fingers (pointing -X / +Z)
        offs[43] = {-0.035f, -0.015f, 0.025f}; // Thumb1
        offs[44] = {-0.030f, -0.005f, 0.015f}; // Thumb2
        offs[45] = {-0.075f, 0.005f, 0.015f};  // Index1
        offs[46] = {-0.040f, 0.000f, 0.002f};  // Index2
        offs[47] = {-0.030f, 0.000f, 0.000f};  // Index3
        offs[48] = {-0.080f, 0.000f, 0.000f};  // Middle1
        offs[49] = {-0.045f, 0.000f, 0.000f};  // Middle2
        offs[50] = {-0.075f, -0.005f, -0.012f};// Ring1
        offs[51] = {-0.038f, 0.000f, 0.000f};  // Ring2
        offs[52] = {-0.028f, 0.000f, 0.000f};  // Ring3
        offs[53] = {-0.065f, -0.010f, -0.022f};// Pinky1
        offs[54] = {-0.032f, 0.000f, 0.000f};  // Pinky2
        offs[55] = {-0.024f, 0.000f, 0.000f};  // Pinky3

        // Toe ends
        offs[56] = {0.0f, -0.010f, 0.070f}; // LeftToeEnd
        offs[57] = {0.0f, -0.010f, 0.070f}; // RightToeEnd
        return offs;
    }();
    return offsets;
}

} // namespace

const std::vector<std::string>& SomaPresentationSpec::jointNames() {
    return getPresentationNames();
}

const std::vector<int>& SomaPresentationSpec::parents() {
    return getPresentationParents();
}

const std::vector<std::array<float, 3>>& SomaPresentationSpec::defaultOffsets() {
    return getPresentationOffsets();
}

int SomaPresentationSpec::jointIndex(const std::string& name) {
    const auto& names = getPresentationNames();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return static_cast<int>(i);
    }
    return -1;
}

bool SomaPresentation::expandSoma30(const Animation& in, Animation& out, std::string& error) {
    if (in.empty() || in.joints < kSomaJoints) {
        error = "Invalid SOMA30 source animation";
        return false;
    }

    const auto& pNames = getPresentationNames();
    const auto& pParents = getPresentationParents();
    const auto& pOffsets = getPresentationOffsets();
    const int targetJoints = static_cast<int>(pNames.size());

    out.frames = in.frames;
    out.fps = in.fps;
    out.joints = targetJoints;
    out.skeletonName = "soma-presentation";
    out.jointNames = pNames;
    out.parents = pParents;
    out.offsets = pOffsets;
    out.rootPositions = in.rootPositions;
    out.localRotationsXyzw.assign(static_cast<size_t>(in.frames) * targetJoints * 4, 0.0f);

    // Map source SOMA joints and set default identity for extended joints
    for (int f = 0; f < in.frames; ++f) {
        const float* srcFrame = in.localRotationsXyzw.data() + static_cast<size_t>(f) * in.joints * 4;
        float* dstFrame = out.localRotationsXyzw.data() + static_cast<size_t>(f) * targetJoints * 4;

        // Copy SOMA30 rotations
        for (int j = 0; j < kSomaJoints; ++j) {
            dstFrame[j * 4 + 0] = srcFrame[j * 4 + 0];
            dstFrame[j * 4 + 1] = srcFrame[j * 4 + 1];
            dstFrame[j * 4 + 2] = srcFrame[j * 4 + 2];
            dstFrame[j * 4 + 3] = srcFrame[j * 4 + 3];
        }

        // Initialize extended finger/toe joints to normalized identity {0, 0, 0, 1}
        for (int j = kSomaJoints; j < targetJoints; ++j) {
            dstFrame[j * 4 + 0] = 0.0f;
            dstFrame[j * 4 + 1] = 0.0f;
            dstFrame[j * 4 + 2] = 0.0f;
            dstFrame[j * 4 + 3] = 1.0f;
        }
    }

    return true;
}

SomaPresentation::ValidationResult SomaPresentation::validate(const Animation& anim) {
    ValidationResult res;
    if (anim.empty()) {
        res.valid = false;
        res.errors.push_back("Animation is empty");
        return res;
    }

    const int J = anim.joints;
    if (static_cast<int>(anim.jointNames.size()) != J ||
        static_cast<int>(anim.parents.size()) != J ||
        static_cast<int>(anim.offsets.size()) != J) {
        res.valid = false;
        res.hierarchyValid = false;
        res.errors.push_back("Hierarchy dimension mismatch");
    }

    // Check root and parents
    int rootCount = 0;
    for (int j = 0; j < J; ++j) {
        const int p = anim.parents[j];
        if (p < 0) {
            rootCount++;
        } else if (p >= j) {
            res.valid = false;
            res.hierarchyValid = false;
            res.errors.push_back("Parent index not strictly less than child index at joint " + std::to_string(j));
        }
    }
    if (rootCount != 1) {
        res.warnings.push_back("Expected exactly 1 root joint, found " + std::to_string(rootCount));
    }

    // Check finite numbers
    for (size_t i = 0; i < anim.localRotationsXyzw.size(); ++i) {
        if (!std::isfinite(anim.localRotationsXyzw[i])) {
            res.valid = false;
            res.isFinite = false;
            res.errors.push_back("Non-finite rotation value found");
            break;
        }
    }
    for (size_t i = 0; i < anim.rootPositions.size(); ++i) {
        if (!std::isfinite(anim.rootPositions[i])) {
            res.valid = false;
            res.isFinite = false;
            res.errors.push_back("Non-finite root position value found");
            break;
        }
    }

    // Left/Right symmetry check
    std::map<std::string, int> nameMap;
    for (int j = 0; j < J; ++j) {
        nameMap[anim.jointNames[j]] = j;
    }
    for (const auto& name : anim.jointNames) {
        if (name.rfind("Left", 0) == 0) {
            std::string rightName = "Right" + name.substr(4);
            if (nameMap.find(rightName) == nameMap.end()) {
                res.leftRightConsistent = false;
                res.warnings.push_back("Missing mirrored right joint for: " + name);
            }
        }
    }

    // Rest pose forward kinematics standing check
    if (res.hierarchyValid && res.isFinite && J > 0) {
        std::vector<float> identRots(static_cast<size_t>(J) * 4, 0.0f);
        for (int j = 0; j < J; ++j) identRots[j * 4 + 3] = 1.0f;
        const float origin[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> worldPos;
        Skeleton::forwardKinematicsGeneral(identRots.data(), origin, anim.parents, anim.offsets, worldPos);

        int headIdx = -1, hipsIdx = -1, footIdx = -1;
        for (int j = 0; j < J; ++j) {
            const std::string& n = anim.jointNames[j];
            if (n == "Head" || n == "head") headIdx = j;
            if (n == "Hips" || n == "hips" || n == "pelvis") hipsIdx = j;
            if (n == "LeftFoot" || n == "foot_l" || n == "LeftFoot_End") footIdx = j;
        }

        if (headIdx >= 0 && hipsIdx >= 0 && footIdx >= 0) {
            res.torsoSpan = worldPos[headIdx].y - worldPos[hipsIdx].y;
            res.standingHeight = worldPos[headIdx].y - worldPos[footIdx].y;
            if (res.torsoSpan < 0.2f || res.standingHeight < 0.8f) {
                res.standingRestPose = false;
                res.warnings.push_back("Rest pose collapsed or non-standing: height=" +
                                       std::to_string(res.standingHeight) + "m");
            }
        }
    }

    return res;
}

} // namespace studio
