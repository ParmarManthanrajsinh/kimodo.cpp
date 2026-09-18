#include "export/ExportPreset.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace studio {

Mat3 Mat3Mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j];
        }
    }
    return r;
}

Mat3 Mat3Transpose(const Mat3& a) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[j][i];
        }
    }
    return r;
}

Quat QuatNormalize(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n > 1e-9f) {
        q.x /= n;
        q.y /= n;
        q.z /= n;
        q.w /= n;
    }
    return q;
}

Mat3 Mat3FromQuat(Quat q) {
    q = QuatNormalize(q);
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

Quat QuatFromMat3(const Mat3& m) {
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
    return QuatNormalize(q);
}

const std::vector<FExportPreset>& exportPresets() {
    static const std::vector<FExportPreset> presets = [] {
        std::vector<FExportPreset> out;
        const Mat3 identity{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

        // First-class BVH Humanoid Preset (Direct pipeline for Unreal Engine IK Rig / Retargeter)
        FExportPreset bvhHumanoid;
        bvhHumanoid.id = "bvh-humanoid";
        bvhHumanoid.name = "BVH Humanoid (Export for Unreal / Maya)";
        bvhHumanoid.profile = "";
        bvhHumanoid.fps = 30.0f;
        bvhHumanoid.scale = 1.0f;
        bvhHumanoid.basis = identity;
        bvhHumanoid.rootMotion = ERootMotion::Preserve;
        bvhHumanoid.format = "BVH";
        out.push_back(bvhHumanoid);

        // Blender Generic Humanoid GLB
        FExportPreset blender;
        blender.id = "blender";
        blender.name = "Blender (generic GLB)";
        blender.profile = "blender-generic";
        blender.fps = 30.0f;
        blender.scale = 1.0f;
        blender.basis = identity;
        blender.rootMotion = ERootMotion::Preserve;
        blender.format = "GLB";
        out.push_back(blender);

        // Generic Raw Animation
        FExportPreset generic;
        generic.id = "generic";
        generic.name = "Generic (as generated)";
        generic.profile = "";
        generic.fps = 30.0f;
        generic.scale = 1.0f;
        generic.basis = identity;
        generic.rootMotion = ERootMotion::Preserve;
        generic.format = "GLB";
        out.push_back(generic);

        return out;
    }();
    return presets;
}

const FExportPreset* findPreset(const std::string& id) {
    for (const FExportPreset& p : exportPresets()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

FAnimation prepareExport(const FAnimation& in, float targetFps, float scale,
                        const Mat3& basis, ERootMotion rootMotion,
                        std::string& report) {
    FAnimation out = in;
    out.fps = targetFps;
    const int J = out.joints;

    // Resample frames if fps changed
    if (std::abs(in.fps - targetFps) > 0.1f && in.fps > 0.0f && targetFps > 0.0f) {
        const float duration = in.GetDuration();
        const int newFrames = std::max(1, static_cast<int>(std::round(duration * targetFps)));
        out.frames = newFrames;
        out.localRotationsXyzw.assign(static_cast<size_t>(newFrames) * J * 4, 0.0f);
        out.rootPositions.assign(static_cast<size_t>(newFrames) * 3, 0.0f);

        for (int f = 0; f < newFrames; ++f) {
            const float t = (newFrames > 1) ? (static_cast<float>(f) / (newFrames - 1) * duration) : 0.0f;
            const float srcFrameF = t * in.fps;
            const int f0 = std::min(in.frames - 1, static_cast<int>(std::floor(srcFrameF)));
            const int f1 = std::min(in.frames - 1, f0 + 1);
            const float alpha = srcFrameF - f0;

            // Interpolate root
            const float* r0 = in.rootPositions.data() + f0 * 3;
            const float* r1 = in.rootPositions.data() + f1 * 3;
            float* dstR = out.rootPositions.data() + f * 3;
            for (int c = 0; c < 3; ++c) {
                dstR[c] = r0[c] * (1.0f - alpha) + r1[c] * alpha;
            }

            // Slerp rotations
            const float* q0 = in.localRotationsXyzw.data() + static_cast<size_t>(f0) * J * 4;
            const float* q1 = in.localRotationsXyzw.data() + static_cast<size_t>(f1) * J * 4;
            float* dstQ = out.localRotationsXyzw.data() + static_cast<size_t>(f) * J * 4;
            for (int j = 0; j < J; ++j) {
                Quat a{q0[j * 4], q0[j * 4 + 1], q0[j * 4 + 2], q0[j * 4 + 3]};
                Quat b{q1[j * 4], q1[j * 4 + 1], q1[j * 4 + 2], q1[j * 4 + 3]};
                // dot product
                float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
                if (dot < 0.0f) {
                    b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
                    dot = -dot;
                }
                Quat res;
                if (dot > 0.9995f) {
                    res = {a.x + alpha * (b.x - a.x), a.y + alpha * (b.y - a.y),
                           a.z + alpha * (b.z - a.z), a.w + alpha * (b.w - a.w)};
                } else {
                    const float theta = std::acos(dot);
                    const float sinTheta = std::sin(theta);
                    const float wa = std::sin((1.0f - alpha) * theta) / sinTheta;
                    const float wb = std::sin(alpha * theta) / sinTheta;
                    res = {a.x * wa + b.x * wb, a.y * wa + b.y * wb,
                           a.z * wa + b.z * wb, a.w * wa + b.w * wb};
                }
                res = QuatNormalize(res);
                dstQ[j * 4 + 0] = res.x;
                dstQ[j * 4 + 1] = res.y;
                dstQ[j * 4 + 2] = res.z;
                dstQ[j * 4 + 3] = res.w;
            }
        }
    }

    // Apply scale & basis transformation to offsets
    const Mat3 basisT = Mat3Transpose(basis);
    for (int j = 0; j < J; ++j) {
        const auto& o = out.offsets[j];
        float vx = o[0] * scale;
        float vy = o[1] * scale;
        float vz = o[2] * scale;
        out.offsets[j][0] = basis.m[0][0] * vx + basis.m[0][1] * vy + basis.m[0][2] * vz;
        out.offsets[j][1] = basis.m[1][0] * vx + basis.m[1][1] * vy + basis.m[1][2] * vz;
        out.offsets[j][2] = basis.m[2][0] * vx + basis.m[2][1] * vy + basis.m[2][2] * vz;
    }

    // Apply basis to rotations & roots
    const float initRootX = out.rootPositions.empty() ? 0.0f : out.rootPositions[0];
    const float initRootZ = out.rootPositions.empty() ? 0.0f : out.rootPositions[2];

    for (int f = 0; f < out.frames; ++f) {
        float* r = out.rootPositions.data() + f * 3;
        float rx = r[0] * scale;
        float ry = r[1] * scale;
        float rz = r[2] * scale;

        if (rootMotion == ERootMotion::LockX) {
            rx = initRootX * scale;
        } else if (rootMotion == ERootMotion::LockXZ) {
            rx = initRootX * scale;
            rz = initRootZ * scale;
        } else if (rootMotion == ERootMotion::Zero) {
            rx = 0.0f;
            ry = 0.0f;
            rz = 0.0f;
        }

        r[0] = basis.m[0][0] * rx + basis.m[0][1] * ry + basis.m[0][2] * rz;
        r[1] = basis.m[1][0] * rx + basis.m[1][1] * ry + basis.m[1][2] * rz;
        r[2] = basis.m[2][0] * rx + basis.m[2][1] * ry + basis.m[2][2] * rz;

        float* qPtr = out.localRotationsXyzw.data() + static_cast<size_t>(f) * J * 4;
        for (int j = 0; j < J; ++j) {
            Quat q{qPtr[j * 4], qPtr[j * 4 + 1], qPtr[j * 4 + 2], qPtr[j * 4 + 3]};
            Mat3 m = Mat3FromQuat(q);
            Mat3 mPrime = Mat3Mul(basis, Mat3Mul(m, basisT));
            Quat qPrime = QuatFromMat3(mPrime);
            qPtr[j * 4 + 0] = qPrime.x;
            qPtr[j * 4 + 1] = qPrime.y;
            qPtr[j * 4 + 2] = qPrime.z;
            qPtr[j * 4 + 3] = qPrime.w;
        }
    }

    std::ostringstream ss;
    ss << "Export prepared: " << out.frames << " frames at " << out.fps << " FPS (scale=" << scale << ")\n";
    report = ss.str();
    return out;
}

} // namespace studio
