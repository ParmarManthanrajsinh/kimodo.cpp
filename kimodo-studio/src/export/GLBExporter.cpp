#include "export/GLBExporter.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace studio
{
namespace
{

void write_u32(std::vector<char>& out, uint32_t v)
{
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

void write_f32(std::vector<char>& out, float v)
{
    uint32_t u = 0;
    static_assert(sizeof(u) == sizeof(v));
    std::memcpy(&u, &v, sizeof(v));
    write_u32(out, u);
}

std::string json_escape(const std::string& s)
{
    std::string o;
    for (char c : s)
    {
        if (c == '"' || c == '\\')
        {
            o += '\\';
        }
        o += c;
    }
    return o;
}

std::string Ftoa(float v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

} // namespace

std::string GLBExporter::DefaultExportDir()
{
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA"))
    {
        return (std::filesystem::path(appdata) / "KimodoStudio" / "exports").string();
    }
#endif
    return "exports";
}

bool GLBExporter::ExportAnimation(const Animation& animation, const ExportOptions& options, std::string& error)
{
    if (animation.empty())
    {
        error = "animation empty";
        return false;
    }
    const int J = animation.joints;
    if (static_cast<int>(animation.parents.size()) != J || static_cast<int>(animation.offsets.size()) != J ||
        static_cast<int>(animation.joint_names.size()) != J)
    {
        error = "animation topology incomplete";
        return false;
    }
    const float fps = options.fps > 0 ? options.fps : animation.fps;
    if (fps <= 0)
    {
        error = "invalid fps";
        return false;
    }

    // Shared preprocessing: resample -> root motion -> basis/scale.
    std::string pre_report;
    const Animation work = PrepareExport(animation, options.fps > 0 ? options.fps : animation.fps, options.root_scale,
                                         options.basis, options.root_motion, pre_report);
    report = pre_report;
    const int F = work.frames;

    // ---- BIN chunk layout ----
    // view 0: times (F floats)
    // views 1..J: joint rotation outputs (F*4 floats each)
    // view J+1: root translation output (F*3 floats)
    std::vector<char> bin;
    auto Pad4 = [&bin] {
        while (bin.size() % 4 != 0)
        {
            bin.push_back(0);
        }
    };
    const size_t times_offset = 0;
    for (int f = 0; f < F; ++f)
    {
        write_f32(bin, static_cast<float>(f) / fps);
    }
    Pad4();
    std::vector<size_t> rot_offsets(J);
    for (int j = 0; j < J; ++j)
    {
        rot_offsets[j] = bin.size();
        for (int f = 0; f < F; ++f)
        {
            const float* q = work.local_rotations_xyzw.data() + (static_cast<size_t>(f) * J + j) * 4;
            write_f32(bin, q[0]);
            write_f32(bin, q[1]);
            write_f32(bin, q[2]);
            write_f32(bin, q[3]);
        }
        Pad4();
    }
    const size_t root_offset = bin.size();
    for (int f = 0; f < F; ++f)
    {
        const float* p = work.root_positions.data() + static_cast<size_t>(f) * 3;
        write_f32(bin, p[0]);
        write_f32(bin, p[1]);
        write_f32(bin, p[2]);
    }
    Pad4();

    // ---- JSON ----
    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Kimodo Studio\"},";
    js << "\"buffers\":[{\"byteLength\":" << bin.size() << "}],";

    // bufferViews
    js << "\"bufferViews\":[";
    js << "{\"buffer\":0,\"byteOffset\":" << times_offset << ",\"byteLength\":" << F * 4 << "}";
    for (int j = 0; j < J; ++j)
    {
        js << ",{\"buffer\":0,\"byteOffset\":" << rot_offsets[j] << ",\"byteLength\":" << F * 16 << "}";
    }
    js << ",{\"buffer\":0,\"byteOffset\":" << root_offset << ",\"byteLength\":" << F * 12 << "}],";

    // accessors: 0 = times, 1..J = rotations, J+1 = root translation
    const float tmax = static_cast<float>(F - 1) / fps;
    js << "\"accessors\":[";
    js << "{\"bufferView\":0,\"componentType\":5126,\"count\":" << F << ",\"type\":\"SCALAR\",\"min\":[0.0],\"max\":["
       << Ftoa(tmax) << "]}";
    for (int j = 0; j < J; ++j)
    {
        js << ",{\"bufferView\":" << (j + 1) << ",\"componentType\":5126,\"count\":" << F << ",\"type\":\"VEC4\"}";
    }
    js << ",{\"bufferView\":" << (J + 1) << ",\"componentType\":5126,\"count\":" << F << ",\"type\":\"VEC3\"}],";

    // nodes with rest offsets + children
    std::vector<std::vector<int>> children(J);
    int root_joint = 0;
    for (int j = 0; j < J; ++j)
    {
        const int p = work.parents[j];
        if (p < 0 || p >= J)
        {
            root_joint = j;
        }
        else
        {
            children[p].push_back(j);
        }
    }
    js << "\"nodes\":[";
    for (int j = 0; j < J; ++j)
    {
        if (j > 0)
        {
            js << ",";
        }
        const auto& o = work.offsets[j];
        js << "{\"name\":\"" << json_escape(work.joint_names[j]) << "\",\"translation\":[" << Ftoa(o[0]) << ","
           << Ftoa(o[1]) << "," << Ftoa(o[2]) << "]";
        if (!children[j].empty())
        {
            js << ",\"children\":[";
            for (size_t k = 0; k < children[j].size(); ++k)
            {
                if (k > 0)
                {
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
    for (int j = 0; j < J; ++j)
    {
        if (j > 0)
        {
            js << ",";
        }
        js << "{\"sampler\":" << j << ",\"target\":{\"node\":" << j << ",\"path\":\"rotation\"}}";
    }
    js << ",{\"sampler\":" << J << ",\"target\":{\"node\":" << root_joint << ",\"path\":\"translation\"}}";
    js << "],\"samplers\":[";
    for (int j = 0; j <= J; ++j)
    {
        if (j > 0)
        {
            js << ",";
        }
        js << "{\"input\":0,\"output\":" << (j + 1) << ",\"interpolation\":\"LINEAR\"}";
    }
    js << "]}],";

    js << "\"scenes\":[{\"nodes\":[" << root_joint << "]}],\"scene\":0}";
    std::string json = js.str();
    while (json.size() % 4 != 0)
    {
        json += ' ';
    }

    // ---- container ----
    const uint32_t json_len = static_cast<uint32_t>(json.size());
    const uint32_t bin_len = static_cast<uint32_t>(bin.size());
    const uint32_t total_len = 12 + 8 + json_len + 8 + bin_len;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(options.path).parent_path(), ec);
    std::ofstream out(options.path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "cannot open output file";
        return false;
    }
    std::vector<char> header;
    write_u32(header, 0x46546C67); // "glTF"
    write_u32(header, 2);
    write_u32(header, total_len);
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    std::vector<char> jh;
    write_u32(jh, json_len);
    write_u32(jh, 0x4E4F534A); // "JSON"
    out.write(jh.data(), static_cast<std::streamsize>(jh.size()));
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    std::vector<char> bh;
    write_u32(bh, bin_len);
    write_u32(bh, 0x004E4942); // "BIN\0"
    out.write(bh.data(), static_cast<std::streamsize>(bh.size()));
    out.write(bin.data(), static_cast<std::streamsize>(bin.size()));
    if (!out)
    {
        error = "write failed";
        return false;
    }
    return true;
}

} // namespace studio
