#include "export/GLBExporter.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace studio {
namespace {

void writeU32(std::vector<char>& out, uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

void writeF32(std::vector<char>& out, float v) {
    uint32_t u = 0;
    static_assert(sizeof(u) == sizeof(v));
    std::memcpy(&u, &v, sizeof(v));
    writeU32(out, u);
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
        }
        o += c;
    }
    return o;
}

std::string ftoa(float v) {
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

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

// Resample animation to target fps (slerp rotations, lerp roots).
Animation resample(const Animation& in, float fps) {
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

std::string GLBExporter::defaultExportDir() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        return (std::filesystem::path(appdata) / "KimodoStudio" / "exports").string();
    }
#endif
    return "exports";
}

bool GLBExporter::exportAnimation(const Animation& animation,
                                  const ExportOptions& options, std::string& error) {
    if (animation.empty()) {
        error = "animation empty";
        return false;
    }
    const int J = animation.joints;
    if (static_cast<int>(animation.parents.size()) != J ||
        static_cast<int>(animation.offsets.size()) != J ||
        static_cast<int>(animation.jointNames.size()) != J) {
        error = "animation topology incomplete";
        return false;
    }
    const float fps = options.fps > 0 ? options.fps : animation.fps;
    if (fps <= 0) {
        error = "invalid fps";
        return false;
    }
    report_.clear();

    // Working copy: resample -> root motion -> basis/scale transform.
    Animation work = resample(animation, options.fps > 0 ? options.fps : animation.fps);
    const int F = work.frames;

    if (options.rootMotion != RootMotion::Preserve) {
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
        if (options.rootMotion == RootMotion::Extract) {
            std::ostringstream rs;
            rs << "extracted root path " << pathLen << " units";
            report_ = rs.str();
        } else {
            report_ = "in-place (root XZ zeroed)";
        }
    }

    const Mat3 c = options.basis;
    const Mat3 ct = mat3Transpose(c);
    auto xformPos = [&](float x, float y, float z, float s, float* out) {
        out[0] = (c.m[0][0] * x + c.m[0][1] * y + c.m[0][2] * z) * s;
        out[1] = (c.m[1][0] * x + c.m[1][1] * y + c.m[1][2] * z) * s;
        out[2] = (c.m[2][0] * x + c.m[2][1] * y + c.m[2][2] * z) * s;
    };
    std::vector<std::array<float, 3>> offsets(J);
    for (int j = 0; j < J; ++j) {
        const auto& o = work.offsets[j];
        float t[3];
        xformPos(o[0], o[1], o[2], options.rootScale, t);
        offsets[j] = {t[0], t[1], t[2]};
    }
    std::vector<float> rots(work.localRotationsXyzw.size());
    for (size_t k = 0; k < rots.size() / 4; ++k) {
        const float* q = work.localRotationsXyzw.data() + k * 4;
        Mat3 r = mat3FromQuat({q[0], q[1], q[2], q[3]});
        Quat nq = quatFromMat3(mat3Mul(mat3Mul(c, r), ct));
        rots[k * 4] = nq.x;
        rots[k * 4 + 1] = nq.y;
        rots[k * 4 + 2] = nq.z;
        rots[k * 4 + 3] = nq.w;
    }
    std::vector<float> roots(static_cast<size_t>(F) * 3);
    for (int f = 0; f < F; ++f) {
        const float* p = work.rootPositions.data() + f * 3;
        xformPos(p[0], p[1], p[2], options.rootScale, roots.data() + f * 3);
    }

    // ---- BIN chunk layout ----
    // view 0: times (F floats)
    // views 1..J: joint rotation outputs (F*4 floats each)
    // view J+1: root translation output (F*3 floats)
    std::vector<char> bin;
    auto pad4 = [&bin] {
        while (bin.size() % 4 != 0) {
            bin.push_back(0);
        }
    };
    const size_t timesOffset = 0;
    for (int f = 0; f < F; ++f) {
        writeF32(bin, static_cast<float>(f) / fps);
    }
    pad4();
    std::vector<size_t> rotOffsets(J);
    for (int j = 0; j < J; ++j) {
        rotOffsets[j] = bin.size();
        for (int f = 0; f < F; ++f) {
            const float* q = rots.data() + (static_cast<size_t>(f) * J + j) * 4;
            writeF32(bin, q[0]);
            writeF32(bin, q[1]);
            writeF32(bin, q[2]);
            writeF32(bin, q[3]);
        }
        pad4();
    }
    const size_t rootOffset = bin.size();
    for (int f = 0; f < F; ++f) {
        const float* p = roots.data() + static_cast<size_t>(f) * 3;
        writeF32(bin, p[0]);
        writeF32(bin, p[1]);
        writeF32(bin, p[2]);
    }
    pad4();

    // ---- JSON ----
    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Kimodo Studio\"},";
    js << "\"buffers\":[{\"byteLength\":" << bin.size() << "}],";

    // bufferViews
    js << "\"bufferViews\":[";
    js << "{\"buffer\":0,\"byteOffset\":" << timesOffset << ",\"byteLength\":" << F * 4
       << "}";
    for (int j = 0; j < J; ++j) {
        js << ",{\"buffer\":0,\"byteOffset\":" << rotOffsets[j] << ",\"byteLength\":"
           << F * 16 << "}";
    }
    js << ",{\"buffer\":0,\"byteOffset\":" << rootOffset << ",\"byteLength\":" << F * 12
       << "}],";

    // accessors: 0 = times, 1..J = rotations, J+1 = root translation
    const float tmax = static_cast<float>(F - 1) / fps;
    js << "\"accessors\":[";
    js << "{\"bufferView\":0,\"componentType\":5126,\"count\":" << F
       << ",\"type\":\"SCALAR\",\"min\":[0.0],\"max\":[" << ftoa(tmax) << "]}";
    for (int j = 0; j < J; ++j) {
        js << ",{\"bufferView\":" << (j + 1)
           << ",\"componentType\":5126,\"count\":" << F << ",\"type\":\"VEC4\"}";
    }
    js << ",{\"bufferView\":" << (J + 1) << ",\"componentType\":5126,\"count\":" << F
       << ",\"type\":\"VEC3\"}],";

    // nodes with rest offsets + children
    std::vector<std::vector<int>> children(J);
    int rootJoint = 0;
    for (int j = 0; j < J; ++j) {
        const int p = animation.parents[j];
        if (p < 0 || p >= J) {
            rootJoint = j;
        } else {
            children[p].push_back(j);
        }
    }
    js << "\"nodes\":[";
    for (int j = 0; j < J; ++j) {
        if (j > 0) {
            js << ",";
        }
        const auto& o = offsets[j];
        js << "{\"name\":\"" << jsonEscape(animation.jointNames[j]) << "\",\"translation\":["
           << ftoa(o[0]) << "," << ftoa(o[1]) << "," << ftoa(o[2]) << "]";
        if (!children[j].empty()) {
            js << ",\"children\":[";
            for (size_t k = 0; k < children[j].size(); ++k) {
                if (k > 0) {
                    js << ",";
                }
                js << children[j][k];
            }
            js << "]";
        }
        js << "}";
    }
    js << "],";

    // one clip: J rotation channels + 1 root translation channel
    js << "\"animations\":[{\"name\":\"KimodoClip\",\"channels\":[";
    for (int j = 0; j < J; ++j) {
        if (j > 0) {
            js << ",";
        }
        js << "{\"sampler\":" << j << ",\"target\":{\"node\":" << j
           << ",\"path\":\"rotation\"}}";
    }
    js << ",{\"sampler\":" << J << ",\"target\":{\"node\":" << rootJoint
       << ",\"path\":\"translation\"}}";
    js << "],\"samplers\":[";
    for (int j = 0; j <= J; ++j) {
        if (j > 0) {
            js << ",";
        }
        js << "{\"input\":0,\"output\":" << (j + 1) << ",\"interpolation\":\"LINEAR\"}";
    }
    js << "]}],";

    js << "\"scenes\":[{\"nodes\":[" << rootJoint << "]}],\"scene\":0}";
    std::string json = js.str();
    while (json.size() % 4 != 0) {
        json += ' ';
    }

    // ---- container ----
    const uint32_t jsonLen = static_cast<uint32_t>(json.size());
    const uint32_t binLen = static_cast<uint32_t>(bin.size());
    const uint32_t totalLen = 12 + 8 + jsonLen + 8 + binLen;

    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(options.path).parent_path(), ec);
    std::ofstream out(options.path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot open output file";
        return false;
    }
    std::vector<char> header;
    writeU32(header, 0x46546C67); // "glTF"
    writeU32(header, 2);
    writeU32(header, totalLen);
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    std::vector<char> jh;
    writeU32(jh, jsonLen);
    writeU32(jh, 0x4E4F534A); // "JSON"
    out.write(jh.data(), static_cast<std::streamsize>(jh.size()));
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    std::vector<char> bh;
    writeU32(bh, binLen);
    writeU32(bh, 0x004E4942); // "BIN\0"
    out.write(bh.data(), static_cast<std::streamsize>(bh.size()));
    out.write(bin.data(), static_cast<std::streamsize>(bin.size()));
    if (!out) {
        error = "write failed";
        return false;
    }
    return true;
}

} // namespace studio
