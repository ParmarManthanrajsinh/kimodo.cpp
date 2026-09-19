#pragma once

#include <memory>
#include <string>
#include <vector>
#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"

namespace studio {

struct CharacterEntry {
    std::string id;
    std::string name;
    std::string file_path;
    std::string license;
    std::string author;
    int bone_count = 0;
    int vertex_count = 0;
    float scale = 1.0f;
    CharacterBoneMap mapping;
    std::string thumbnail_path;
    bool installed = true;
};

class CharacterLibrary {
public:
    CharacterLibrary() = default;

    bool Init();
    void Rescan();

    const std::vector<CharacterEntry>& GetEntries() const { return entries; }
    const std::string& GetActiveId() const { return active_id; }

    CharacterAsset* GetActiveAsset() { return active_asset.get(); }
    const CharacterAsset* GetActiveAsset() const { return active_asset.get(); }

    bool SelectCharacter(const std::string& id);
    bool ImportCharacter(const std::string& source_path, std::string& error);
    bool RemoveCharacter(const std::string& id);

    bool SaveMapping(const std::string& id, const CharacterBoneMap& mapping);
    bool FindEntry(const std::string& id, CharacterEntry& out_entry) const;

private:
    std::vector<CharacterEntry> entries;
    std::string active_id;
    std::unique_ptr<CharacterAsset> active_asset;

    void LoadRegistry();
    void SaveRegistry();
    void RegisterDefaultCharacters();
};

} // namespace studio
