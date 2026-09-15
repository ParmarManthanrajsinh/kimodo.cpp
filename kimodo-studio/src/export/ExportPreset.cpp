#include "export/ExportPreset.h"

#include <algorithm>
#include <cmath>
#include <sstream>

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

        // First-class UE pipeline: generic humanoid BVH. Import into UE,
        // build IK Rig + IK Retargeter there. No Manny-specific encoding.
        ExportPreset bvhHumanoid;
        bvhHumanoid.id = "bvh-humanoid";
        bvhHumanoid.name = "BVH Humanoid";
        bvhHumanoid.profile = "";
        bvhHumanoid.fps = 30.0f;
        bvhHumanoid.scale = 1.0f;
        bvhHumanoid.basis = identity;
        bvhHumanoid.rootMotion = RootMotion::Preserve;
        bvhHumanoid.format = "BVH";
        out.push_back(bvhHumanoid);

        // Unity: Y-up left-handed. Mirror Z (matches UniGLTF-style import).
        ExportPreset unity;
        unity.id = "unity";
        unity.name = "Unity";
        unity.profile = "unity-humanoid";
        unity.fps = 30.0f;
        unity.scale = 1.0f; // meters
        unity.basis = Mat3{{{1, 0, 0}, {0, 1, 0}, {0, 0, -1}}};
        out.push_back(unity);

        // Deprecated: internal UE Manny retarget removed. Use bvh-humanoid
        // + UE IK Retargeter instead. Kept for backward compat only.
        // Unreal: Z-up left-handed, centimeters. glTF (x,y,z) -> UE (z,x,y).
        ExportPreset unreal;
        unreal.id = "unreal";
        unreal.name = "Unreal Engine (deprecated: use BVH Humanoid)";
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

namespace {

Quat slerp(Quat a, Quat b, float t) {
    a = quatNormalize(a);
    b = quatNormalize(b);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0) {
        dot = -dot;
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }
    if (dot > 0.9995f) {
        Quat r{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
               a.w + (b.w - a.w) * t};
        return quatNormalize(r);
    }
    const float theta = std::acos(dot);
    const float s = std::sin(theta);
    const float wa = std::sin((1 - t) * theta) / s;
    const float wb = std::sin(t * theta) / s;
    Quat r{a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb,
           a.w * wa + b.w * wb};
    return quatNormalize(r);
}

Animation resampleAnim(const Animation& in, float fps) {
    if (fps <= 0 || std::abs(fps - in.fps) < 1e-6f || in.frames < 2) {
        return in;
    }
    const float dur = in.duration();
    const int outFrames = std::max(2, static_cast<int>(std::round(dur * fps)));
    Animation out = in;
    out.fps = fps;
    out.frames = outFrames;
    out.localRotationsXyzw.assign(static_cast<size_t>(outFrames) * in.joints * 4, 0);
    out.rootPositions.assign(static_cast<size_t>(outFrames) * 3, 0);
    for (int f = 0; f < outFrames; ++f) {
        const float t = std::min(dur, static_cast<float>(f) / fps);
        const float srcF = t * in.fps;
        const int i0 = std::min(static_cast<int>(srcF), in.frames - 1);
        const int i1 = std::min(i0 + 1, in.frames - 1);
        const float a = std::min(1.0f, std::max(0.0f, srcF - i0));
        for (int j = 0; j < in.joints; ++j) {
            const float* r0 = in.localRotationsXyzw.data() + (i0 * in.joints + j) * 4;
            const float* r1 = in.localRotationsXyzw.data() + (i1 * in.joints + j) * 4;
            Quat q = slerp({r0[0], r0[1], r0[2], r0[3]}, {r1[0], r1[1], r1[2], r1[3]}, a);
            float* d = out.localRotationsXyzw.data() + (f * in.joints + j) * 4;
            d[0] = q.x;
            d[1] = q.y;
            d[2] = q.z;
            d[3] = q.w;
        }
        const float* p0 = in.rootPositions.data() + i0 * 3;
        const float* p1 = in.rootPositions.data() + i1 * 3;
        float* d = out.rootPositions.data() + f * 3;
        for (int k = 0; k < 3; ++k) {
            d[k] = p0[k] + (p1[k] - p0[k]) * a;
        }
    }
    return out;
}

} // namespace

Animation prepareExport(const Animation& in, float fps, float scale, Mat3 basis,
                        RootMotion rootMotion, std::string& report) {
    report.clear();
    Animation work = resampleAnim(in, fps > 0 ? fps : in.fps);
    const int F = work.frames;
    const int J = work.joints;

    if (rootMotion != RootMotion::Preserve) {
        float pathLen = 0.0f;
        float px = work.rootPositions[0];
        float pz = work.rootPositions[2];
        for (int f = 0; f < F; ++f) {
            float* p = work.rootPositions.data() + f * 3;
            if (f > 0) {
                const float dx = p[0] - px;
                const float dz = p[2] - pz;
                pathLen += std::sqrt(dx * dx + dz * dz);
                px = p[0];
                pz = p[2];
            }
            p[0] = 0.0f;
            p[2] = 0.0f;
        }
        if (rootMotion == RootMotion::Extract) {
            std::ostringstream rs;
            rs << "extracted root path " << pathLen << " units";
            report = rs.str();
        } else {
            report = "in-place (root XZ zeroed)";
        }
    }

    // Identity fast path: no float churn when nothing transforms.
    bool identity = scale == 1.0f;
    for (int i = 0; identity && i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (basis.m[i][j] != (i == j ? 1.0f : 0.0f)) {
                identity = false;
            }
        }
    }
    if (identity) {
        return work;
    }

    const Mat3 ct = mat3Transpose(basis);
    auto xformPos = [&](float x, float y, float z, float* out) {
        out[0] = (basis.m[0][0] * x + basis.m[0][1] * y + basis.m[0][2] * z) * scale;
        out[1] = (basis.m[1][0] * x + basis.m[1][1] * y + basis.m[1][2] * z) * scale;
        out[2] = (basis.m[2][0] * x + basis.m[2][1] * y + basis.m[2][2] * z) * scale;
    };
    for (int j = 0; j < J; ++j) {
        const auto& o = work.offsets[j];
        float t[3];
        xformPos(o[0], o[1], o[2], t);
        work.offsets[j] = {t[0], t[1], t[2]};
    }
    for (size_t k = 0; k < work.localRotationsXyzw.size() / 4; ++k) {
        const float* q = work.localRotationsXyzw.data() + k * 4;
        Mat3 r = mat3FromQuat({q[0], q[1], q[2], q[3]});
        Quat nq = quatFromMat3(mat3Mul(mat3Mul(basis, r), ct));
        float* d = work.localRotationsXyzw.data() + k * 4;
        d[0] = nq.x;
        d[1] = nq.y;
        d[2] = nq.z;
        d[3] = nq.w;
    }
    for (int f = 0; f < F; ++f) {
        const float* p = work.rootPositions.data() + f * 3;
        float t[3];
        xformPos(p[0], p[1], p[2], t);
        float* d = work.rootPositions.data() + f * 3;
        d[0] = t[0];
        d[1] = t[1];
        d[2] = t[2];
    }
    return work;
}

} // namespace studio
