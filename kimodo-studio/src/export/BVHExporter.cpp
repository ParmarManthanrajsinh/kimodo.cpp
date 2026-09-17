#include "export/BVHExporter.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace studio {

void BVHExporter::quatToEulerXYZ(float x, float y, float z, float w, float& ex, float& ey,
                                 float& ez) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w)) {
        ex = ey = ez = 0.0f;
        return;
    }
    const float n = std::sqrt(x * x + y * y + z * z + w * w);
    if (n > 1e-9f) {
        x /= n;
        y /= n;
        z /= n;
        w /= n;
    } else {
        ex = ey = ez = 0.0f;
        return;
    }
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;
    const float m20 = 2 * (xz - wy);
    const float m21 = 2 * (yz + wx);
    const float m22 = 1 - 2 * (xx + yy);
    const float m10 = 2 * (xy + wz);
    const float m00 = 1 - 2 * (yy + zz);
    const float m12 = 2 * (yz - wx);
    const float m11 = 1 - 2 * (xx + zz);
    constexpr float kDeg = 57.29577951308232f;
    ey = std::asin(std::clamp(-m20, -1.0f, 1.0f)) * kDeg;
    if (std::abs(m20) < 0.99999f) {
        ex = std::atan2(m21, m22) * kDeg;
        ez = std::atan2(m10, m00) * kDeg;
    } else {
        // Gimbal lock singularity handling
        ex = std::atan2(-m12, m11) * kDeg;
        ez = 0.0f;
    }
}

namespace {

std::string ftoa(float v) {
    if (!std::isfinite(v)) return "0.0";
    std::ostringstream ss;
    ss.precision(6);
    ss << std::fixed << v;
    std::string str = ss.str();
    // Trim trailing zeros after decimal point
    if (str.find('.') != std::string::npos) {
        while (str.back() == '0') str.pop_back();
        if (str.back() == '.') str.push_back('0');
    }
    return str;
}

} // namespace

bool BVHExporter::exportAnimation(const Animation& animation,
                                  const ExportOptions& options, std::string& error) {
    return exportWithRange(animation, options, -1, -1, error, &report_);
}

bool BVHExporter::exportWithRange(const Animation& animation, const ExportOptions& options,
                                  int startFrame, int endFrame, std::string& error,
                                  std::string* report) {
    if (animation.empty()) {
        error = "Animation is empty";
        return false;
    }
    const int J = animation.joints;
    if (static_cast<int>(animation.parents.size()) != J ||
        static_cast<int>(animation.offsets.size()) != J ||
        static_cast<int>(animation.jointNames.size()) != J) {
        error = "Animation hierarchy incomplete";
        return false;
    }
    const float fps = options.fps > 0 ? options.fps : animation.fps;
    if (fps <= 0) {
        error = "Invalid FPS";
        return false;
    }

    std::string preReport;
    const Animation work = prepareExport(animation, fps, options.rootScale,
                                         options.basis, options.rootMotion, preReport);
    if (report) *report = preReport;

    const int totalF = work.frames;
    int f0 = (startFrame >= 0 && startFrame < totalF) ? startFrame : 0;
    int f1 = (endFrame >= f0 && endFrame < totalF) ? endFrame : (totalF - 1);
    const int outFrames = (f1 - f0 + 1);

    std::vector<std::vector<int>> children(J);
    int rootJoint = 0;
    for (int j = 0; j < J; ++j) {
        const int p = work.parents[j];
        if (p < 0 || p >= J) {
            rootJoint = j;
        } else {
            children[p].push_back(j);
        }
    }

    // Verify depth-first ordering
    {
        std::vector<int> order;
        std::vector<int> stack{rootJoint};
        while (!stack.empty()) {
            const int j = stack.back();
            stack.pop_back();
            order.push_back(j);
            for (auto it = children[j].rbegin(); it != children[j].rend(); ++it) {
                stack.push_back(*it);
            }
        }
        for (int j = 0; j < J; ++j) {
            if (order[j] != j) {
                error = "Joint order is not depth-first preorder";
                return false;
            }
        }
    }

    std::ostringstream out;
    out << "HIERARCHY\n";

    struct FrameItem {
        int joint;
        size_t childIdx;
        bool opened;
    };
    std::vector<FrameItem> stack{{rootJoint, 0, false}};
    auto indent = [](int depth) { return std::string(static_cast<size_t>(depth) * 2, ' '); };

    while (!stack.empty()) {
        FrameItem& fr = stack.back();
        const int depth = static_cast<int>(stack.size()) - 1;
        const int j = fr.joint;
        if (!fr.opened) {
            fr.opened = true;
            const auto& o = work.offsets[j];
            if (depth == 0) {
                out << "ROOT " << work.jointNames[j] << "\n"
                    << indent(depth + 1) << "{\n"
                    << indent(depth + 2) << "OFFSET " << ftoa(o[0]) << " "
                    << ftoa(o[1]) << " " << ftoa(o[2]) << "\n"
                    << indent(depth + 2)
                    << "CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation\n";
            } else {
                out << indent(depth) << "JOINT " << work.jointNames[j] << "\n"
                    << indent(depth) << "{\n"
                    << indent(depth + 1) << "OFFSET " << ftoa(o[0]) << " "
                    << ftoa(o[1]) << " " << ftoa(o[2]) << "\n"
                    << indent(depth + 1) << "CHANNELS 3 Xrotation Yrotation Zrotation\n";
            }
        }
        if (fr.childIdx < children[j].size()) {
            const int c = children[j][fr.childIdx++];
            stack.push_back({c, 0, false});
        } else {
            if (children[j].empty()) {
                const auto& o = work.offsets[j];
                float len = std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
                float dx = 0, dy = -1, dz = 0;
                if (len > 1e-6f) {
                    dx = o[0] / len;
                    dy = o[1] / len;
                    dz = o[2] / len;
                }
                const float ext = std::max(0.05f, len * 0.25f);
                out << indent(depth + 1) << "End Site\n"
                    << indent(depth + 1) << "{\n"
                    << indent(depth + 2) << "OFFSET " << ftoa(dx * ext) << " "
                    << ftoa(dy * ext) << " " << ftoa(dz * ext) << "\n"
                    << indent(depth + 1) << "}\n";
            }
            out << indent(depth) << "}\n";
            stack.pop_back();
        }
    }

    out << "MOTION\nFrames: " << outFrames << "\nFrame Time: " << ftoa(1.0f / fps) << "\n";
    for (int f = f0; f <= f1; ++f) {
        bool first = true;
        for (int j = 0; j < J; ++j) {
            if (!first) out << " ";
            first = false;
            if (j == rootJoint) {
                const float* p = work.rootPositions.data() + f * 3;
                out << ftoa(p[0]) << " " << ftoa(p[1]) << " " << ftoa(p[2]) << " ";
            }
            const float* q = work.localRotationsXyzw.data() + (static_cast<size_t>(f) * J + j) * 4;
            float ex, ey, ez;
            quatToEulerXYZ(q[0], q[1], q[2], q[3], ex, ey, ez);
            out << ftoa(ex) << " " << ftoa(ey) << " " << ftoa(ez);
        }
        out << "\n";
    }

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(options.path).parent_path(), ec);
    std::ofstream file(options.path, std::ios::trunc);
    if (!file) {
        error = "Cannot open output file: " + options.path;
        return false;
    }
    file << out.str();
    if (!file) {
        error = "Failed writing BVH output to " + options.path;
        return false;
    }
    return true;
}

} // namespace studio
