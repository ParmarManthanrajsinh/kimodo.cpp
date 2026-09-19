#include "export/ExportPreset.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace studio {

Mat3 Mat3Mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
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

const std::vector<ExportPreset>& export_presets() {
    static const std::vector<ExportPreset> presets = [] {
        std::vector<ExportPreset> out;
        const Mat3 identity{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

        // First-class BVH Humanoid Preset (Direct pipeline for Unreal Engine IK Rig / Retargeter)
        ExportPreset bvh_humanoid;
        bvh_humanoid.id = "bvh-humanoid";
        bvh_humanoid.name = "BVH Humanoid (Export for Unreal / Maya)";
        bvh_humanoid.profile = "";
        bvh_humanoid.fps = 30.0f;
        bvh_humanoid.scale = 1.0f;
        bvh_humanoid.basis = identity;
        bvh_humanoid.root_motion = RootMotion::Preserve;
        bvh_humanoid.format = "BVH";
        out.push_back(bvh_humanoid);

        // Blender Generic Humanoid GLB
        ExportPreset blender;
        blender.id = "blender";
        blender.name = "Blender (generic GLB)";
        blender.profile = "blender-generic";
        blender.fps = 30.0f;
        blender.scale = 1.0f;
        blender.basis = identity;
        blender.root_motion = RootMotion::Preserve;
        blender.format = "GLB";
        out.push_back(blender);

        // Generic Raw Animation
        ExportPreset generic;
        generic.id = "generic";
        generic.name = "Generic (as generated)";
        generic.profile = "";
        generic.fps = 30.0f;
        generic.scale = 1.0f;
        generic.basis = identity;
        generic.root_motion = RootMotion::Preserve;
        generic.format = "GLB";
        out.push_back(generic);

        return out;
    }();
    return presets;
}

const ExportPreset* FindPreset(const std::string& id) {
    for (const ExportPreset& p : export_presets()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

Animation PrepareExport(const Animation& in, float target_fps, float scale, const Mat3& basis, RootMotion root_motion,
                        std::string& report) {
    Animation out = in;
    out.fps = target_fps;
    const int J = out.joints;

    // Resample frames if fps changed
    if (std::abs(in.fps - target_fps) > 0.1f && in.fps > 0.0f && target_fps > 0.0f) {
        const float duration = in.GetDuration();
        const int new_frames = std::max(1, static_cast<int>(std::round(duration * target_fps)));
        out.frames = new_frames;
        out.local_rotations_xyzw.assign(static_cast<size_t>(new_frames) * J * 4, 0.0f);
        out.root_positions.assign(static_cast<size_t>(new_frames) * 3, 0.0f);

        for (int f = 0; f < new_frames; ++f) {
            const float t = (new_frames > 1) ? (static_cast<float>(f) / (new_frames - 1) * duration) : 0.0f;
            const float src_frame_f = t * in.fps;
            const int f0 = std::min(in.frames - 1, static_cast<int>(std::floor(src_frame_f)));
            const int f1 = std::min(in.frames - 1, f0 + 1);
            const float alpha = src_frame_f - f0;

            // Interpolate root
            const float* r0 = in.root_positions.data() + f0 * 3;
            const float* r1 = in.root_positions.data() + f1 * 3;
            float* dst_r = out.root_positions.data() + f * 3;
            for (int c = 0; c < 3; ++c) {
                dst_r[c] = r0[c] * (1.0f - alpha) + r1[c] * alpha;
            }

            // Slerp rotations
            const float* q0 = in.local_rotations_xyzw.data() + static_cast<size_t>(f0) * J * 4;
            const float* q1 = in.local_rotations_xyzw.data() + static_cast<size_t>(f1) * J * 4;
            float* dst_q = out.local_rotations_xyzw.data() + static_cast<size_t>(f) * J * 4;
            for (int j = 0; j < J; ++j) {
                Quat a{q0[j * 4], q0[j * 4 + 1], q0[j * 4 + 2], q0[j * 4 + 3]};
                Quat b{q1[j * 4], q1[j * 4 + 1], q1[j * 4 + 2], q1[j * 4 + 3]};
                // dot product
                float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
                if (dot < 0.0f) {
                    b.x = -b.x;
                    b.y = -b.y;
                    b.z = -b.z;
                    b.w = -b.w;
                    dot = -dot;
                }
                Quat res;
                if (dot > 0.9995f) {
                    res = {a.x + alpha * (b.x - a.x), a.y + alpha * (b.y - a.y), a.z + alpha * (b.z - a.z),
                           a.w + alpha * (b.w - a.w)};
                } else {
                    const float theta = std::acos(dot);
                    const float sin_theta = std::sin(theta);
                    const float wa = std::sin((1.0f - alpha) * theta) / sin_theta;
                    const float wb = std::sin(alpha * theta) / sin_theta;
                    res = {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
                }
                res = QuatNormalize(res);
                dst_q[j * 4 + 0] = res.x;
                dst_q[j * 4 + 1] = res.y;
                dst_q[j * 4 + 2] = res.z;
                dst_q[j * 4 + 3] = res.w;
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
    const float init_root_x = out.root_positions.empty() ? 0.0f : out.root_positions[0];
    const float init_root_z = out.root_positions.empty() ? 0.0f : out.root_positions[2];

    for (int f = 0; f < out.frames; ++f) {
        float* r = out.root_positions.data() + f * 3;
        float rx = r[0] * scale;
        float ry = r[1] * scale;
        float rz = r[2] * scale;

        if (root_motion == RootMotion::LockX) {
            rx = init_root_x * scale;
        } else if (root_motion == RootMotion::LockXZ) {
            rx = init_root_x * scale;
            rz = init_root_z * scale;
        } else if (root_motion == RootMotion::Zero) {
            rx = 0.0f;
            ry = 0.0f;
            rz = 0.0f;
        }

        r[0] = basis.m[0][0] * rx + basis.m[0][1] * ry + basis.m[0][2] * rz;
        r[1] = basis.m[1][0] * rx + basis.m[1][1] * ry + basis.m[1][2] * rz;
        r[2] = basis.m[2][0] * rx + basis.m[2][1] * ry + basis.m[2][2] * rz;

        float* q_ptr = out.local_rotations_xyzw.data() + static_cast<size_t>(f) * J * 4;
        for (int j = 0; j < J; ++j) {
            Quat q{q_ptr[j * 4], q_ptr[j * 4 + 1], q_ptr[j * 4 + 2], q_ptr[j * 4 + 3]};
            Mat3 m = Mat3FromQuat(q);
            Mat3 m_prime = Mat3Mul(basis, Mat3Mul(m, basisT));
            Quat q_prime = QuatFromMat3(m_prime);
            q_ptr[j * 4 + 0] = q_prime.x;
            q_ptr[j * 4 + 1] = q_prime.y;
            q_ptr[j * 4 + 2] = q_prime.z;
            q_ptr[j * 4 + 3] = q_prime.w;
        }
    }

    std::ostringstream ss;
    ss << "Export prepared: " << out.frames << " frames at " << out.fps << " FPS (scale=" << scale << ")\n";
    report = ss.str();
    return out;
}

} // namespace studio
