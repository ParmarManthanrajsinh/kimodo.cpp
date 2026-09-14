#include "library/AnimationLibrary.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
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

bool AnimationLibrary::save(const std::string& prompt, const std::string& model, float fps,
                            const MotionResult& motion, LibraryEntry& out) {
    static int counter = 0;
    out.id = "anim-" + nowStamp() + "-" + std::to_string(++counter);
    out.prompt = prompt;
    out.model = model;
    out.createdAt = nowStamp();
    out.fps = fps;
    out.frames = motion.frames;
    out.joints = motion.joints;
    out.dir = base_ / out.id;

    std::error_code ec;
    std::filesystem::create_directories(out.dir, ec);
    std::ofstream bin(out.dir / "motion.bin", std::ios::binary | std::ios::trunc);
    if (!bin) {
        return false;
    }
    const uint32_t frames = static_cast<uint32_t>(motion.frames);
    const uint32_t joints = static_cast<uint32_t>(motion.joints);
    bin.write(reinterpret_cast<const char*>(&frames), sizeof(frames));
    bin.write(reinterpret_cast<const char*>(&joints), sizeof(joints));
    bin.write(reinterpret_cast<const char*>(&fps), sizeof(fps));
    bin.write(reinterpret_cast<const char*>(motion.localRotationsXyzw.data()),
              static_cast<std::streamsize>(motion.localRotationsXyzw.size() * sizeof(float)));
    bin.write(reinterpret_cast<const char*>(motion.rootPositions.data()),
              static_cast<std::streamsize>(motion.rootPositions.size() * sizeof(float)));
    if (!bin || !writeMetadata(out.dir, out)) {
        return false;
    }
    entries_.push_back(out);
    return true;
}

bool AnimationLibrary::loadMotion(const LibraryEntry& entry, MotionResult& out) const {
    std::ifstream bin(entry.dir / "motion.bin", std::ios::binary);
    if (!bin) {
        return false;
    }
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
    out.localRotationsXyzw.resize(static_cast<size_t>(frames) * joints * 4);
    out.rootPositions.resize(static_cast<size_t>(frames) * 3);
    bin.read(reinterpret_cast<char*>(out.localRotationsXyzw.data()),
             static_cast<std::streamsize>(out.localRotationsXyzw.size() * sizeof(float)));
    bin.read(reinterpret_cast<char*>(out.rootPositions.data()),
             static_cast<std::streamsize>(out.rootPositions.size() * sizeof(float)));
    return static_cast<bool>(bin);
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
            MotionResult m;
            if (!loadMotion(e, m)) {
                return false;
            }
            LibraryEntry copy;
            return save(e.prompt, e.model, e.fps, m, copy);
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
