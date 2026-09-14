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

std::string nowStamp() {
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

// Minimal readers for files we wrote ourselves (flat keys only).
bool findString(const std::string& json, const std::string& key, std::string& out) {
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

bool findNumber(const std::string& json, const std::string& key, double& out) {
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

std::filesystem::path AnimationLibrary::defaultBaseDir() {
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

bool AnimationLibrary::init(const std::filesystem::path& baseDir) {
    base_ = baseDir;
    std::error_code ec;
    std::filesystem::create_directories(base_, ec);
    rescan();
    return true;
}

void AnimationLibrary::rescan() {
    entries_.clear();
    std::error_code ec;
    for (const auto& dir : std::filesystem::directory_iterator(base_, ec)) {
        if (!dir.is_directory()) {
            continue;
        }
        LibraryEntry e;
        if (readMetadata(dir.path(), e)) {
            entries_.push_back(std::move(e));
        }
    }
}

bool AnimationLibrary::writeMetadata(const std::filesystem::path& dir,
                                     const LibraryEntry& e) {
    std::ofstream out(dir / "metadata.json", std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "{\"id\":\"" << jsonEscape(e.id) << "\",\"prompt\":\"" << jsonEscape(e.prompt)
        << "\",\"model\":\"" << jsonEscape(e.model) << "\",\"created_at\":\""
        << jsonEscape(e.createdAt) << "\",\"fps\":" << e.fps << ",\"frames\":" << e.frames
        << ",\"joints\":" << e.joints << ",\"skeleton\":\"" << jsonEscape(e.skeleton)
        << "\",\"file\":\"motion.bin\"}";
    return static_cast<bool>(out);
}

bool AnimationLibrary::readMetadata(const std::filesystem::path& dir, LibraryEntry& e) {
    std::ifstream in(dir / "metadata.json");
    if (!in) {
        return false;
    }
    const std::string json{std::istreambuf_iterator<char>(in), {}};
    double num = 0;
    if (!findString(json, "id", e.id) || !findString(json, "prompt", e.prompt)) {
        return false;
    }
    findString(json, "model", e.model);
    findString(json, "created_at", e.createdAt);
    findString(json, "skeleton", e.skeleton);
    if (findNumber(json, "fps", num)) {
        e.fps = static_cast<float>(num);
    }
    if (findNumber(json, "frames", num)) {
        e.frames = static_cast<int>(num);
    }
    if (findNumber(json, "joints", num)) {
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
    out.localRotationsXyzw.resize(static_cast<size_t>(frames) * joints * 4);
    out.rootPositions.resize(static_cast<size_t>(frames) * 3);
    bin.read(reinterpret_cast<char*>(out.localRotationsXyzw.data()),
             static_cast<std::streamsize>(out.localRotationsXyzw.size() * sizeof(float)));
    bin.read(reinterpret_cast<char*>(out.rootPositions.data()),
             static_cast<std::streamsize>(out.rootPositions.size() * sizeof(float)));
    return static_cast<bool>(bin);
}

void writeU32(std::ofstream& bin, uint32_t v) {
    bin.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

bool readU32(std::ifstream& bin, uint32_t& v) {
    bin.read(reinterpret_cast<char*>(&v), sizeof(v));
    return static_cast<bool>(bin);
}

} // namespace

bool AnimationLibrary::saveAnimation(const std::string& prompt, const std::string& model,
                                     const Animation& anim, LibraryEntry& out) {
    // Unique across restarts: timestamp + ms + random (no shared counter).
    static thread_local std::mt19937 rng{std::random_device{}()};
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count() %
                    1000;
    std::ostringstream idss;
    idss << "anim-" << nowStamp() << "-" << std::setfill('0') << std::setw(3) << ms << "-"
         << std::setfill('0') << std::setw(4) << (rng() % 10000);
    out.id = idss.str();
    out.prompt = prompt;
    out.model = model;
    out.createdAt = nowStamp();
    out.fps = anim.fps;
    out.frames = anim.frames;
    out.joints = anim.joints;
    out.skeleton = anim.skeletonName;
    out.dir = base_ / out.id;

    std::error_code ec;
    std::filesystem::create_directories(out.dir, ec);
    std::ofstream bin(out.dir / "motion.bin", std::ios::binary | std::ios::trunc);
    if (!bin) {
        return false;
    }
    bin.write("KAMD", 4);
    writeU32(bin, 2); // version
    writeU32(bin, static_cast<uint32_t>(anim.frames));
    writeU32(bin, static_cast<uint32_t>(anim.joints));
    bin.write(reinterpret_cast<const char*>(&anim.fps), sizeof(anim.fps));
    writeU32(bin, static_cast<uint32_t>(anim.jointNames.size()));
    for (const std::string& n : anim.jointNames) {
        writeU32(bin, static_cast<uint32_t>(n.size()));
        bin.write(n.data(), static_cast<std::streamsize>(n.size()));
    }
    writeU32(bin, static_cast<uint32_t>(anim.parents.size()));
    for (int p : anim.parents) {
        const int32_t v = p;
        bin.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
    writeU32(bin, static_cast<uint32_t>(anim.offsets.size()));
    for (const auto& o : anim.offsets) {
        bin.write(reinterpret_cast<const char*>(o.data()),
                  static_cast<std::streamsize>(3 * sizeof(float)));
    }
    bin.write(reinterpret_cast<const char*>(anim.localRotationsXyzw.data()),
              static_cast<std::streamsize>(anim.localRotationsXyzw.size() * sizeof(float)));
    bin.write(reinterpret_cast<const char*>(anim.rootPositions.data()),
              static_cast<std::streamsize>(anim.rootPositions.size() * sizeof(float)));
    if (!bin || !writeMetadata(out.dir, out)) {
        return false;
    }
    entries_.push_back(out);
    return true;
}

bool AnimationLibrary::loadAnimation(const LibraryEntry& entry, Animation& out) const {
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
        soma.skeletonName = "soma30";
        soma.jointNames.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
        soma.parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
        soma.offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
        soma.localRotationsXyzw = std::move(out.localRotationsXyzw);
        soma.rootPositions = std::move(out.rootPositions);
        out = std::move(soma);
        return true;
    }
    uint32_t version = 0;
    if (!readU32(bin, version) || version != 2) {
        return false;
    }
    if (!readAnimData(bin, out)) {
        return false;
    }
    uint32_t count = 0;
    if (!readU32(bin, count) || count > 512) {
        return false;
    }
    out.jointNames.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = 0;
        if (!readU32(bin, len) || len > 256) {
            return false;
        }
        out.jointNames[i].resize(len);
        bin.read(out.jointNames[i].data(), len);
        if (!bin) {
            return false;
        }
    }
    if (!readU32(bin, count) || count > 512) {
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
    if (!readU32(bin, count) || count > 512) {
        return false;
    }
    out.offsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        bin.read(reinterpret_cast<char*>(out.offsets[i].data()),
                 static_cast<std::streamsize>(3 * sizeof(float)));
        if (!bin) {
            return false;
        }
    }
    return true;
}

bool AnimationLibrary::rename(const std::string& id, const std::string& newPrompt) {
    for (LibraryEntry& e : entries_) {
        if (e.id == id) {
            e.prompt = newPrompt;
            return writeMetadata(e.dir, e);
        }
    }
    return false;
}

bool AnimationLibrary::duplicate(const std::string& id) {
    for (const LibraryEntry& e : entries_) {
        if (e.id == id) {
            Animation anim;
            if (!loadAnimation(e, anim)) {
                return false;
            }
            LibraryEntry copy;
            return saveAnimation(e.prompt, e.model, anim, copy);
        }
    }
    return false;
}

bool AnimationLibrary::hasThumb(const LibraryEntry& e) {
    std::error_code ec;
    return std::filesystem::is_regular_file(e.dir / "thumb.png", ec);
}

bool AnimationLibrary::remove(const std::string& id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->id == id) {
            std::error_code ec;
            std::filesystem::remove_all(it->dir, ec);
            entries_.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace studio
