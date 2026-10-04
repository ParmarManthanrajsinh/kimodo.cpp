#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "animation/Animation.h"

namespace studio
{

struct LibraryEntry
{
    std::string id;
    std::string prompt;
    std::string model;
    std::string created_at;
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
class AnimationLibrary
{
public:
    bool Init(const std::filesystem::path& base_dir);
    void Rescan();

    [[nodiscard]] const std::vector<LibraryEntry>& GetEntries() const noexcept
    {
        return entries;
    }

    bool SaveAnimation(std::string_view prompt, std::string_view model, const Animation& anim, LibraryEntry& out);
    [[nodiscard]] bool LoadAnimation(const LibraryEntry& entry, Animation& out) const;
    bool Rename(std::string_view id, std::string_view new_prompt);
    bool duplicate(std::string_view id);
    bool Remove(std::string_view id);

    [[nodiscard]] static std::filesystem::path thumb_path(const LibraryEntry& e)
    {
        return e.dir / "thumb.png";
    }
    [[nodiscard]] static bool has_thumb(const LibraryEntry& e);

    [[nodiscard]] static std::filesystem::path default_base_dir();

private:
    static bool WriteMetadata(const std::filesystem::path& dir, const LibraryEntry& e);
    static bool ReadMetadata(const std::filesystem::path& dir, LibraryEntry& e);

    std::filesystem::path base_dir;
    std::vector<LibraryEntry> entries;
};

} // namespace studio
