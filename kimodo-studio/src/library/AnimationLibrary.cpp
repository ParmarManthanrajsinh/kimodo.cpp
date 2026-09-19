#include "library/AnimationLibrary.h"

#include "animation/Skeleton.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace studio {
namespace {

std::string now_stamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return ss.str();
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
        }
        o += c;
    }
    return o;
}

// Minimal readers for files we wrote ourselves (flat keys only).
bool find_string(const std::string& json, const std::string& key, std::string& out) {
    const std::string pat = "\"" + key + "\":\"";
    const size_t p = json.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    const size_t s = p + pat.size();
    const size_t e = json.find('"', s);
    if (e == std::string::npos) {
        return false;
    }
    out = json.substr(s, e - s);
    return true;
}

bool find_number(const std::string& json, const std::string& key, double& out) {
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    try {
        out = std::stod(json.substr(p + pat.size()));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

std::filesystem::path AnimationLibrary::default_base_dir() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(appdata) / "KimodoStudio" / "animations";
    }
    return std::filesystem::path("animations");
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".kimodo-studio" / "animations";
    }
    return std::filesystem::path("animations");
#endif
}

bool AnimationLibrary::Init(const std::filesystem::path& in_base_dir) {
    base_dir = in_base_dir;
    std::error_code ec;
    std::filesystem::create_directories(base_dir, ec);
    Rescan();
    return true;
}

void AnimationLibrary::Rescan() {
    entries.clear();
    std::error_code ec;
    for (const auto& dir : std::filesystem::directory_iterator(base_dir, ec)) {
        if (!dir.is_directory()) {
            continue;
        }
        LibraryEntry e;
        if (ReadMetadata(dir.path(), e)) {
            entries.push_back(std::move(e));
        }
    }
}

bool AnimationLibrary::WriteMetadata(const std::filesystem::path& dir, const LibraryEntry& e) {
    std::ofstream out(dir / "metadata.json", std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "{\"id\":\"" << json_escape(e.id) << "\",\"prompt\":\"" << json_escape(e.prompt) << "\",\"model\":\""
        << json_escape(e.model) << "\",\"created_at\":\"" << json_escape(e.created_at) << "\",\"fps\":" << e.fps
        << ",\"frames\":" << e.frames << ",\"joints\":" << e.joints << ",\"skeleton\":\"" << json_escape(e.skeleton)
        << "\",\"file\":\"motion.bin\"}";
    return static_cast<bool>(out);
}

bool AnimationLibrary::ReadMetadata(const std::filesystem::path& dir, LibraryEntry& e) {
    std::ifstream in(dir / "metadata.json");
    if (!in) {
        return false;
    }
    const std::string json{std::istreambuf_iterator<char>(in), {}};
    double num = 0;
    if (!find_string(json, "id", e.id) || !find_string(json, "prompt", e.prompt)) {
        return false;
    }
    find_string(json, "model", e.model);
    find_string(json, "created_at", e.created_at);
    find_string(json, "skeleton", e.skeleton);
    if (find_number(json, "fps", num)) {
        e.fps = static_cast<float>(num);
    }
    if (find_number(json, "frames", num)) {
        e.frames = static_cast<int>(num);
    }
    if (find_number(json, "joints", num)) {
        e.joints = static_cast<int>(num);
    }
    e.dir = dir;
    return true;
}

namespace {

bool readAnimData(std::ifstream& bin, Animation& out) {
    uint32_t frames = 0, joints = 0;
    float fps = 30.0f;
    bin.read(reinterpret_cast<char*>(&frames), sizeof(frames));
    bin.read(reinterpret_cast<char*>(&joints), sizeof(joints));
    bin.read(reinterpret_cast<char*>(&fps), sizeof(fps));
    if (!bin || frames == 0 || joints == 0 || frames > 100000 || joints > 256) {
        return false;
    }
    out.frames = static_cast<int>(frames);
    out.joints = static_cast<int>(joints);
    out.fps = fps;
    out.local_rotations_xyzw.resize(static_cast<size_t>(frames) * joints * 4);
    out.root_positions.resize(static_cast<size_t>(frames) * 3);
    bin.read(reinterpret_cast<char*>(out.local_rotations_xyzw.data()),
             static_cast<std::streamsize>(out.local_rotations_xyzw.size() * sizeof(float)));
    bin.read(reinterpret_cast<char*>(out.root_positions.data()),
             static_cast<std::streamsize>(out.root_positions.size() * sizeof(float)));
    return static_cast<bool>(bin);
}

void write_u32(std::ofstream& bin, uint32_t v) { bin.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

bool read_u32(std::ifstream& bin, uint32_t& v) {
    bin.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(bin);
}

} // namespace

bool AnimationLibrary::SaveAnimation(const std::string& prompt, const std::string& model, const Animation& anim,
                                     LibraryEntry& out) {
    // Unique across restarts: timestamp + ms + random (no shared counter).
    static thread_local std::mt19937 rng{std::random_device{}()};
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count() %
        1000;
    std::ostringstream idss;
    idss << "anim-" << now_stamp() << "-" << std::setfill('0') << std::setw(3) << ms << "-" << std::setfill('0')
         << std::setw(4) << (rng() % 10000);
    out.id = idss.str();
    out.prompt = prompt;
    out.model = model;
    out.created_at = now_stamp();
    out.fps = anim.fps;
    out.frames = anim.frames;
    out.joints = anim.joints;
    out.skeleton = anim.skeleton_name;
    out.dir = base_dir / out.id;

    std::error_code ec;
    std::filesystem::create_directories(out.dir, ec);
    std::ofstream bin(out.dir / "motion.bin", std::ios::binary | std::ios::trunc);
    if (!bin) {
        return false;
    }
    bin.write("KAMD", 4);
    write_u32(bin, 2); // version
    write_u32(bin, static_cast<uint32_t>(anim.frames));
    write_u32(bin, static_cast<uint32_t>(anim.joints));
    bin.write(reinterpret_cast<const char*>(&anim.fps), sizeof(anim.fps));
    write_u32(bin, static_cast<uint32_t>(anim.joint_names.size()));
    for (const std::string& n : anim.joint_names) {
        write_u32(bin, static_cast<uint32_t>(n.size()));
        bin.write(n.data(), static_cast<std::streamsize>(n.size()));
    }
    write_u32(bin, static_cast<uint32_t>(anim.parents.size()));
    for (int p : anim.parents) {
        const int32_t v = p;
        bin.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
    write_u32(bin, static_cast<uint32_t>(anim.offsets.size()));
    for (const auto& o : anim.offsets) {
        bin.write(reinterpret_cast<const char*>(o.data()), static_cast<std::streamsize>(3 * sizeof(float)));
    }
    bin.write(reinterpret_cast<const char*>(anim.local_rotations_xyzw.data()),
              static_cast<std::streamsize>(anim.local_rotations_xyzw.size() * sizeof(float)));
    bin.write(reinterpret_cast<const char*>(anim.root_positions.data()),
              static_cast<std::streamsize>(anim.root_positions.size() * sizeof(float)));
    if (!bin || !WriteMetadata(out.dir, out)) {
        return false;
    }
    entries.push_back(out);
    return true;
}

bool AnimationLibrary::LoadAnimation(const LibraryEntry& entry, Animation& out) const {
    std::ifstream bin(entry.dir / "motion.bin", std::ios::binary);
    if (!bin) {
        return false;
    }
    char magic[4] = {};
    bin.read(magic, 4);
    if (!bin) {
        return false;
    }
    if (std::string(magic, 4) != "KAMD") {
        // Legacy v1: rewind, topology defaults to SOMA.
        bin.clear();
        bin.seekg(0);
        if (!readAnimData(bin, out)) {
            return false;
        }
        Animation soma;
        soma.frames = out.frames;
        soma.joints = out.joints;
        soma.fps = out.fps;
        soma.skeleton_name = "soma30";
        soma.joint_names.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
        soma.parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
        soma.offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
        soma.local_rotations_xyzw = std::move(out.local_rotations_xyzw);
        soma.root_positions = std::move(out.root_positions);
        out = std::move(soma);
        return true;
    }
    // v2 layout (must match saveAnimation): version, frames, joints, fps,
    // joint names, parents, offsets, rotations, root positions.
    uint32_t version = 0;
    uint32_t frames = 0, joints = 0;
    float fps = 30.0f;
    if (!read_u32(bin, version) || version != 2) {
        return false;
    }
    bin.read(reinterpret_cast<char*>(&frames), sizeof(frames));
    bin.read(reinterpret_cast<char*>(&joints), sizeof(joints));
    bin.read(reinterpret_cast<char*>(&fps), sizeof(fps));
    if (!bin || frames == 0 || joints == 0 || frames > 100000 || joints > 512 || fps <= 0 || fps > 1000) {
        return false;
    }
    out.frames = static_cast<int>(frames);
    out.joints = static_cast<int>(joints);
    out.fps = fps;
    uint32_t count = 0;
    if (!read_u32(bin, count) || count != joints) {
        return false;
    }
    out.joint_names.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = 0;
        if (!read_u32(bin, len) || len == 0 || len > 256) {
            return false;
        }
        out.joint_names[i].resize(len);
        bin.read(out.joint_names[i].data(), len);
        if (!bin) {
            return false;
        }
    }
    if (!read_u32(bin, count) || count != joints) {
        return false;
    }
    out.parents.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        int32_t v = 0;
        bin.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (!bin) {
            return false;
        }
        out.parents[i] = v;
    }
    if (!read_u32(bin, count) || count != joints) {
        return false;
    }
    out.offsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        bin.read(reinterpret_cast<char*>(out.offsets[i].data()), static_cast<std::streamsize>(3 * sizeof(float)));
        if (!bin) {
            return false;
        }
    }
    out.local_rotations_xyzw.resize(static_cast<size_t>(frames) * joints * 4);
    out.root_positions.resize(static_cast<size_t>(frames) * 3);
    bin.read(reinterpret_cast<char*>(out.local_rotations_xyzw.data()),
             static_cast<std::streamsize>(out.local_rotations_xyzw.size() * sizeof(float)));
    bin.read(reinterpret_cast<char*>(out.root_positions.data()),
             static_cast<std::streamsize>(out.root_positions.size() * sizeof(float)));
    if (!bin) {
        return false;
    }
    out.skeleton_name = entry.skeleton;
    return true;
}

bool AnimationLibrary::Rename(const std::string& id, const std::string& new_prompt) {
    for (LibraryEntry& e : entries) {
        if (e.id == id) {
            e.prompt = new_prompt;
            return WriteMetadata(e.dir, e);
        }
    }
    return false;
}

bool AnimationLibrary::duplicate(const std::string& id) {
    for (const LibraryEntry& e : entries) {
        if (e.id == id) {
            Animation anim;
            if (!LoadAnimation(e, anim)) {
                return false;
            }
            LibraryEntry copy;
            return SaveAnimation(e.prompt, e.model, anim, copy);
        }
    }
    return false;
}

bool AnimationLibrary::has_thumb(const LibraryEntry& e) {
    std::error_code ec;
    return std::filesystem::is_regular_file(e.dir / "thumb.png", ec);
}

bool AnimationLibrary::Remove(const std::string& id) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->id == id) {
            std::error_code ec;
            std::filesystem::remove_all(it->dir, ec);
            entries.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace studio
