#pragma once

#include "animation/Animation.h"
#include <array>
#include <string>
#include <vector>

namespace studio {

enum class ERootMotion { Preserve, LockX, LockXZ, Zero };

struct Mat3 {
    float m[3][3];
};

struct Quat {
    float x, y, z, w;
};

Mat3 Mat3Mul(const Mat3& a, const Mat3& b);
Mat3 Mat3Transpose(const Mat3& a);
Quat QuatNormalize(Quat q);
Mat3 Mat3FromQuat(Quat q);
Quat QuatFromMat3(const Mat3& m);

struct FExportPreset {
    std::string id;       // "bvh-humanoid", "blender", "generic"
    std::string name;     // "BVH Humanoid (for Unreal / DCC)", "Blender"
    std::string profile;  // "blender-generic" or ""
    float fps = 30.0f;
    float scale = 1.0f;
    Mat3 basis{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    ERootMotion rootMotion = ERootMotion::Preserve;
    std::string format = "GLB"; // "GLB" or "BVH"
};

const std::vector<FExportPreset>& exportPresets();
const FExportPreset* findPreset(const std::string& id);

FAnimation prepareExport(const FAnimation& in, float targetFps, float scale,
                        const Mat3& basis, ERootMotion rootMotion,
                        std::string& report);

} // namespace studio
