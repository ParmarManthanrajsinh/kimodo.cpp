#include "export/ExportPreset.h"

#include <cmath>

namespace studio {

Mat3 mat3Mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j];
        }
    }
    return r;
}

Mat3 mat3Transpose(const Mat3& a) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[j][i];
        }
    }
    return r;
}

Quat quatNormalize(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n > 1e-9f) {
        q.x /= n;
        q.y /= n;
        q.z /= n;
        q.w /= n;
    }
    return q;
}

Mat3 mat3FromQuat(Quat q) {
    q = quatNormalize(q);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    Mat3 r{};
    r.m[0][0] = 1 - 2 * (yy + zz);
    r.m[0][1] = 2 * (xy - wz);
    r.m[0][2] = 2 * (xz + wy);
    r.m[1][0] = 2 * (xy + wz);
    r.m[1][1] = 1 - 2 * (xx + zz);
    r.m[1][2] = 2 * (yz - wx);
    r.m[2][0] = 2 * (xz - wy);
    r.m[2][1] = 2 * (yz + wx);
    r.m[2][2] = 1 - 2 * (xx + yy);
    return r;
}

Quat quatFromMat3(const Mat3& m) {
    const float t = m.m[0][0] + m.m[1][1] + m.m[2][2];
    Quat q{0, 0, 0, 1};
    if (t > 0) {
        const float s = 2 * std::sqrt(t + 1);
        q.w = 0.25f * s;
        q.x = (m.m[2][1] - m.m[1][2]) / s;
        q.y = (m.m[0][2] - m.m[2][0]) / s;
        q.z = (m.m[1][0] - m.m[0][1]) / s;
    } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
        const float s = 2 * std::sqrt(1 + m.m[0][0] - m.m[1][1] - m.m[2][2]);
        q.w = (m.m[2][1] - m.m[1][2]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[0][1] + m.m[1][0]) / s;
        q.z = (m.m[0][2] + m.m[2][0]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        const float s = 2 * std::sqrt(1 + m.m[1][1] - m.m[0][0] - m.m[2][2]);
        q.w = (m.m[0][2] - m.m[2][0]) / s;
        q.x = (m.m[0][1] + m.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[1][2] + m.m[2][1]) / s;
    } else {
        const float s = 2 * std::sqrt(1 + m.m[2][2] - m.m[0][0] - m.m[1][1]);
        q.w = (m.m[1][0] - m.m[0][1]) / s;
        q.x = (m.m[0][2] + m.m[2][0]) / s;
        q.y = (m.m[1][2] + m.m[2][1]) / s;
        q.z = 0.25f * s;
    }
    return quatNormalize(q);
}

const std::vector<ExportPreset>& exportPresets() {
    static const std::vector<ExportPreset> presets = [] {
        std::vector<ExportPreset> out;
        const Mat3 identity{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

        ExportPreset blender;
        blender.id = "blender";
        blender.name = "Blender";
        blender.profile = "blender-generic";
        blender.fps = 30.0f;
        blender.scale = 1.0f;
        blender.basis = identity;
        out.push_back(blender);

        // Unity: Y-up left-handed. Mirror Z (matches UniGLTF-style import).
        ExportPreset unity;
        unity.id = "unity";
        unity.name = "Unity";
        unity.profile = "unity-humanoid";
        unity.fps = 30.0f;
        unity.scale = 1.0f; // meters
        unity.basis = Mat3{{{1, 0, 0}, {0, 1, 0}, {0, 0, -1}}};
        out.push_back(unity);

        // Unreal: Z-up left-handed, centimeters. glTF (x,y,z) -> UE (z,x,y).
        ExportPreset unreal;
        unreal.id = "unreal";
        unreal.name = "Unreal Engine";
        unreal.profile = "unreal-manny";
        unreal.fps = 30.0f;
        unreal.scale = 100.0f; // meters -> cm
        unreal.basis = Mat3{{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}};
        out.push_back(unreal);

        ExportPreset generic;
        generic.id = "generic";
        generic.name = "Generic (as generated)";
        generic.profile = "";
        generic.fps = 30.0f;
        generic.scale = 1.0f;
        generic.basis = identity;
        out.push_back(generic);

        return out;
    }();
    return presets;
}

const ExportPreset* findPreset(const std::string& id) {
    for (const ExportPreset& p : exportPresets()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace studio
