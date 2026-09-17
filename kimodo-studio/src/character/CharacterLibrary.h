#pragma once

#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace studio {

struct CharacterEntry {
    std::string id;
    std::string name;
    std::string filePath;
    std::string license;
    std::string author;
    int boneCount = 0;
    int vertexCount = 0;
    float scale = 1.0f;
    CharacterBoneMap mapping;
    std::string thumbnailPath;
    bool installed = true;
};

class CharacterLibrary {
public:
    CharacterLibrary() = default;

    bool init();
    void rescan();

    const std::vector<CharacterEntry>& entries() const { return entries_; }
    const std::string& activeId() const { return activeId_; }

    CharacterAsset* activeAsset() { return activeAsset_.get(); }
    const CharacterAsset* activeAsset() const { return activeAsset_.get(); }

    bool selectCharacter(const std::string& id);
    bool importCharacter(const std::string& sourcePath, std::string& error);
    bool removeCharacter(const std::string& id);

    bool saveMapping(const std::string& id, const CharacterBoneMap& mapping);
    bool findEntry(const std::string& id, CharacterEntry& outEntry) const;

private:
    std::vector<CharacterEntry> entries_;
    std::string activeId_;
    std::unique_ptr<CharacterAsset> activeAsset_;

    void loadRegistry();
    void saveRegistry();
    void registerDefaultCharacters();
};

} // namespace studio
