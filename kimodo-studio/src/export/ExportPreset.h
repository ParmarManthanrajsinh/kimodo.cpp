#pragma once

#include <array>
#include <string>
#include <vector>

namespace studio {

// Small 3x3 + quaternion math for export-time basis conversion (no external
// dep; Studio-local, independent of inference code).
struct Mat3 {
    float m[3][3];
};

struct Quat {
    float x, y, z, w;
};

Mat3 mat3Mul(const Mat3& a, const Mat3& b);
Mat3 mat3Transpose(const Mat3& a);
Quat quatFromMat3(const Mat3& m);
Mat3 mat3FromQuat(Quat q);
Quat quatNormalize(Quat q);

enum class RootMotion { Preserve, InPlace, Extract };

struct ExportPreset {
    std::string id;        // "unreal"
    std::string name;      // "Unreal Engine"
    std::string profile;   // retarget profile id ("" = keep source skeleton)
    float fps = 30.0f;
    float scale = 1.0f;    // unit scale into target (Unreal cm = 100)
    Mat3 basis;            // applied to positions; rotations via C*R*C'
    RootMotion rootMotion = RootMotion::Preserve;
    std::string format = "GLB";
};

const std::vector<ExportPreset>& exportPresets();
const ExportPreset* findPreset(const std::string& id);

} // namespace studio
