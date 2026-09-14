#include "export/GLBExporter.h"

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

    // Shared preprocessing: resample -> root motion -> basis/scale.
    std::string preReport;
    const Animation work = prepareExport(animation, options.fps > 0 ? options.fps : animation.fps,
                                         options.rootScale, options.basis,
                                         options.rootMotion, preReport);
    report_ = preReport;
    const int F = work.frames;

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
            const float* q =
                work.localRotationsXyzw.data() + (static_cast<size_t>(f) * J + j) * 4;
            writeF32(bin, q[0]);
            writeF32(bin, q[1]);
            writeF32(bin, q[2]);
            writeF32(bin, q[3]);
        }
        pad4();
    }
    const size_t rootOffset = bin.size();
    for (int f = 0; f < F; ++f) {
        const float* p = work.rootPositions.data() + static_cast<size_t>(f) * 3;
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
        const int p = work.parents[j];
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
        const auto& o = work.offsets[j];
        js << "{\"name\":\"" << jsonEscape(work.jointNames[j]) << "\",\"translation\":["
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
