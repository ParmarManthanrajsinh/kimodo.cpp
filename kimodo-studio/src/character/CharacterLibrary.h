#pragma once

#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace studio {

struct FCharacterEntry {
    std::string id;
    std::string name;
    std::string filePath;
    std::string license;
    std::string author;
    int boneCount = 0;
    int vertexCount = 0;
    float scale = 1.0f;
    FCharacterBoneMap mapping;
    std::string thumbnailPath;
    bool installed = true;
};

class FCharacterLibrary {
public:
    FCharacterLibrary() = default;

    bool Init();
    void Rescan();

    const std::vector<FCharacterEntry>& GetEntries() const { return entries; }
    const std::string& GetActiveId() const { return ActiveId; }

    FCharacterAsset* GetActiveAsset() { return ActiveAsset.get(); }
    const FCharacterAsset* GetActiveAsset() const { return ActiveAsset.get(); }

    bool SelectCharacter(const std::string& id);
    bool ImportCharacter(const std::string& sourcePath, std::string& error);
    bool RemoveCharacter(const std::string& id);

    bool SaveMapping(const std::string& id, const FCharacterBoneMap& mapping);
    bool FindEntry(const std::string& id, FCharacterEntry& outEntry) const;

private:
    std::vector<FCharacterEntry> entries;
    std::string ActiveId;
    std::unique_ptr<FCharacterAsset> ActiveAsset;

    void LoadRegistry();
    void SaveRegistry();
    void RegisterDefaultCharacters();
};

} // namespace studio
