#pragma once

#include <array>
#include <string>
#include <vector>
#include "animation/Animation.h"

namespace studio
{

enum class RootMotion
{
    Preserve,
    LockX,
    LockXZ,
    Zero
};

struct Mat3
{
    float m[3][3];
};

struct Quat
{
    float x, y, z, w;
};

Mat3 Mat3Mul(const Mat3& a, const Mat3& b);
Mat3 Mat3Transpose(const Mat3& a);
Quat QuatNormalize(Quat q);
Mat3 Mat3FromQuat(Quat q);
Quat QuatFromMat3(const Mat3& m);

struct ExportPreset
{
    std::string id;      // "bvh-humanoid", "blender", "generic"
    std::string name;    // "BVH Humanoid (for Unreal / DCC)", "Blender"
    std::string profile; // "blender-generic" or ""
    float fps = 30.0f;
    float scale = 1.0f;
    Mat3 basis{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    RootMotion root_motion = RootMotion::Preserve;
    std::string format = "GLB"; // "GLB" or "BVH"
};

const std::vector<ExportPreset>& export_presets();
const ExportPreset* FindPreset(const std::string& id);

Animation PrepareExport(const Animation& in, float target_fps, float scale, const Mat3& basis, RootMotion root_motion,
                        std::string& report);

} // namespace studio
