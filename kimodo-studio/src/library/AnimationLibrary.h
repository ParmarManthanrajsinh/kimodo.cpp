#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "kimodo/KimodoAdapter.h" // MotionResult

namespace studio {

struct LibraryEntry {
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
// motion.bin: u32 frames, u32 joints, f32 fps, rots[], roots[].
class AnimationLibrary {
public:
    bool init(const std::filesystem::path& baseDir);
    void rescan();

    const std::vector<LibraryEntry>& entries() const { return entries_; }

    bool save(const std::string& prompt, const std::string& model, float fps,
              const MotionResult& motion, LibraryEntry& out);
    bool loadMotion(const LibraryEntry& entry, MotionResult& out) const;
    bool rename(const std::string& id, const std::string& newPrompt);
    bool duplicate(const std::string& id);
    bool remove(const std::string& id);

    static std::filesystem::path defaultBaseDir();

private:
    static bool writeMetadata(const std::filesystem::path& dir, const LibraryEntry& e);
    static bool readMetadata(const std::filesystem::path& dir, LibraryEntry& e);

    std::filesystem::path base_;
    std::vector<LibraryEntry> entries_;
};

} // namespace studio
