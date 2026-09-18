#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "animation/Animation.h"

namespace studio {

struct FLibraryEntry {
    std::string id;
    std::string prompt;
    std::string model;
    std::string createdAt;
    float fps = 30.0f;
    int frames = 0;
    int joints = 0;
    std::string skeleton = "soma30";
    std::filesystem::path dir;
};

// Persisted animation library (plan section 11):
//   animations/<id>/metadata.json + motion.bin
// motion.bin v1 (legacy): u32 frames, u32 joints, f32 fps, rots[], roots[].
// motion.bin v2: "KAMD", u32 version=2, frames, joints, f32 fps,
//   topology (names, parents, offsets), rots[], roots[].
class FAnimationLibrary {
public:
    bool Init(const std::filesystem::path& baseDir);
    void Rescan();

    const std::vector<FLibraryEntry>& GetEntries() const { return entries; }

    bool saveAnimation(const std::string& prompt, const std::string& model,
                       const FAnimation& anim, FLibraryEntry& out);
    bool loadAnimation(const FLibraryEntry& entry, FAnimation& out) const;
    bool rename(const std::string& id, const std::string& newPrompt);
    bool duplicate(const std::string& id);
    bool remove(const std::string& id);

    static std::filesystem::path thumbPath(const FLibraryEntry& e) {
        return e.dir / "thumb.png";
    }
    static bool hasThumb(const FLibraryEntry& e);

    static std::filesystem::path defaultBaseDir();

private:
    static bool writeMetadata(const std::filesystem::path& dir, const FLibraryEntry& e);
    static bool readMetadata(const std::filesystem::path& dir, FLibraryEntry& e);

    std::filesystem::path Base;
    std::vector<FLibraryEntry> entries;
};

} // namespace studio
