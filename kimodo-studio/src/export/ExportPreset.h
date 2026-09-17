#pragma once

#include "animation/Animation.h"
#include <array>
#include <string>
#include <vector>

namespace studio {

enum class RootMotion { Preserve, LockX, LockXZ, Zero };

struct Mat3 {
    float m[3][3];
};

struct Quat {
    float x, y, z, w;
};

Mat3 mat3Mul(const Mat3& a, const Mat3& b);
Mat3 mat3Transpose(const Mat3& a);
Quat quatNormalize(Quat q);
Mat3 mat3FromQuat(Quat q);
Quat quatFromMat3(const Mat3& m);

struct ExportPreset {
    std::string id;       // "bvh-humanoid", "blender", "generic"
    std::string name;     // "BVH Humanoid (for Unreal / DCC)", "Blender"
    std::string profile;  // "blender-generic" or ""
    float fps = 30.0f;
    float scale = 1.0f;
    Mat3 basis{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    RootMotion rootMotion = RootMotion::Preserve;
    std::string format = "GLB"; // "GLB" or "BVH"
};

const std::vector<ExportPreset>& exportPresets();
const ExportPreset* findPreset(const std::string& id);

Animation prepareExport(const Animation& in, float targetFps, float scale,
                        const Mat3& basis, RootMotion rootMotion,
                        std::string& report);

} // namespace studio
