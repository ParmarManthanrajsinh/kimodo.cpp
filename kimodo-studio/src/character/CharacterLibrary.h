#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "character/CharacterAsset.h"
#include "character/CharacterMapper.h"

namespace studio
{

struct CharacterEntry
{
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

class CharacterLibrary
{
public:
    CharacterLibrary() = default;

    bool Init();
    void Rescan();

    [[nodiscard]] const std::vector<CharacterEntry>& GetEntries() const noexcept
    {
        return entries;
    }
    [[nodiscard]] const std::string& GetActiveId() const noexcept
    {
        return active_id;
    }

    [[nodiscard]] CharacterAsset* GetActiveAsset() noexcept
    {
        return active_asset.get();
    }
    [[nodiscard]] const CharacterAsset* GetActiveAsset() const noexcept
    {
        return active_asset.get();
    }

    bool SelectCharacter(std::string_view id);
    bool ImportCharacter(std::string_view source_path, std::string& error);
    bool RemoveCharacter(std::string_view id);

    bool SaveMapping(std::string_view id, const CharacterBoneMap& mapping);
    bool FindEntry(std::string_view id, CharacterEntry& out_entry) const;

private:
    std::vector<CharacterEntry> entries;
    std::string active_id;
    std::unique_ptr<CharacterAsset> active_asset;

    void LoadRegistry();
    void SaveRegistry();
    void RegisterDefaultCharacters();
};

} // namespace studio
