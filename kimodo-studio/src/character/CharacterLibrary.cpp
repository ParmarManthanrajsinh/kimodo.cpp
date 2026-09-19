#include "character/CharacterLibrary.h"
#include "character/CharacterLoader.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace studio {
namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

} // namespace

bool CharacterLibrary::Init() {
    AppPaths::EnsureDirectories();
    LoadRegistry();
    RegisterDefaultCharacters();
    if (!entries.empty()) {
        SelectCharacter(entries.front().id);
    }
    return true;
}

void CharacterLibrary::RegisterDefaultCharacters() {
    // Check if CesiumMan already registered
    auto it =
        std::find_if(entries.begin(), entries.end(), [](const CharacterEntry& e) { return e.id == "cesium-man"; });

    const std::filesystem::path cesium_path = AppPaths::ResolveAsset("assets/characters/CesiumMan.glb");
    std::error_code ec;
    const std::vector<std::string> dummy_source = {
        "Hips",         "Spine1",    "Spine2",      "Chest",       "Neck1",         "Head",
        "LeftShoulder", "LeftArm",   "LeftForeArm", "LeftHand",    "RightShoulder", "RightArm",
        "RightForeArm", "RightHand", "LeftLeg",     "LeftShin",    "LeftFoot",      "LeftToeBase",
        "RightLeg",     "RightShin", "RightFoot",   "RightToeBase"};

    if (std::filesystem::is_regular_file(cesium_path, ec) && !ec) {
        if (it == entries.end()) {
            CharacterAsset temp;
            std::string err;
            if (CharacterLoader::LoadGLB(cesium_path.string(), temp, err)) {
                CharacterEntry entry;
                entry.id = "cesium-man";
                entry.name = "Cesium Man (glTF Sample)";
                entry.file_path = cesium_path.string();
                entry.license = "CC-BY 4.0";
                entry.author = "Cesium (Khronos glTF Sample Assets)";
                entry.bone_count = static_cast<int>(temp.GetBones().size());
                entry.vertex_count = static_cast<int>(temp.GetSkinningData().vertices.size());
                entry.scale = 1.0f;
                entry.mapping = CharacterMapper::AutoMap(temp, dummy_source);
                entries.push_back(std::move(entry));
                SaveRegistry();
            }
        } else {
            // Re-evaluate mapping in case alias tables improved
            CharacterAsset temp;
            std::string err;
            if (CharacterLoader::LoadGLB(cesium_path.string(), temp, err)) {
                it->mapping = CharacterMapper::AutoMap(temp, dummy_source);
                it->bone_count = static_cast<int>(temp.GetBones().size());
                it->vertex_count = static_cast<int>(temp.GetSkinningData().vertices.size());
                SaveRegistry();
            }
        }
    }
}

void CharacterLibrary::LoadRegistry() {
    entries.clear();
    const auto reg_path = AppPaths::CharacterRegistryFile();
    std::ifstream file(reg_path);
    if (!file.is_open())
        return;

    // Minimal JSON array reader
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    size_t pos = 0;
    while ((pos = text.find("{\"id\":\"", pos)) != std::string::npos) {
        CharacterEntry e;
        size_t id_end = text.find('"', pos + 7);
        if (id_end != std::string::npos) {
            e.id = text.substr(pos + 7, id_end - (pos + 7));
        }

        // Simple property extraction
        auto get_prop = [&](const std::string& key) -> std::string {
            std::string pat = "\"" + key + "\":\"";
            size_t p = text.find(pat, pos);
            if (p != std::string::npos && p < text.find("}", pos)) {
                size_t start = p + pat.size();
                size_t end = text.find('"', start);
                if (end != std::string::npos)
                    return text.substr(start, end - start);
            }
            return "";
        };

        e.name = get_prop("name");
        e.file_path = get_prop("file_path");
        e.license = get_prop("license");
        e.author = get_prop("author");

        std::error_code ec;
        e.installed = std::filesystem::exists(e.file_path, ec) && !ec;

        if (!e.id.empty()) {
            entries.push_back(e);
        }
        pos = text.find('}', pos);
        if (pos == std::string::npos)
            break;
        pos++;
    }
}

void CharacterLibrary::SaveRegistry() {
    const auto reg_path = AppPaths::CharacterRegistryFile();
    std::ofstream file(reg_path, std::ios::trunc);
    if (!file.is_open())
        return;

    file << "[\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        file << "  {\n"
             << "    \"id\": \"" << json_escape(e.id) << "\",\n"
             << "    \"name\": \"" << json_escape(e.name) << "\",\n"
             << "    \"file_path\": \"" << json_escape(e.file_path) << "\",\n"
             << "    \"license\": \"" << json_escape(e.license) << "\",\n"
             << "    \"author\": \"" << json_escape(e.author) << "\",\n"
             << "    \"boneCount\": " << e.bone_count << ",\n"
             << "    \"vertexCount\": " << e.vertex_count << ",\n"
             << "    \"scale\": " << e.scale << "\n"
             << "  }" << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    file << "]\n";
}

bool CharacterLibrary::SelectCharacter(const std::string& id) {
    auto it = std::find_if(entries.begin(), entries.end(), [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries.end())
        return false;

    auto asset = std::make_unique<CharacterAsset>();
    std::string err;
    if (!CharacterLoader::LoadGLB(it->file_path, *asset, err)) {
        Logger::GetInstance().Error("Failed loading character " + id + ": " + err);
        return false;
    }

    active_id = id;
    active_asset = std::move(asset);
    Logger::GetInstance().Info("Selected character: " + it->name + " (" + id + ")");
    return true;
}

bool CharacterLibrary::ImportCharacter(const std::string& source_path, std::string& error) {
    std::filesystem::path src(source_path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(src, ec) || ec) {
        error = "File does not exist: " + source_path;
        return false;
    }

    // Copy to user characters directory
    const std::filesystem::path dest = AppPaths::DefaultCharactersDir() / src.filename();
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);

    CharacterAsset test_asset;
    if (!CharacterLoader::LoadGLB(dest.string(), test_asset, error)) {
        return false;
    }

    CharacterEntry entry;
    entry.id = src.stem().string();
    entry.name = src.stem().string();
    entry.file_path = dest.string();
    entry.license = "User Imported";
    entry.author = "User";
    entry.bone_count = static_cast<int>(test_asset.GetBones().size());
    entry.vertex_count = static_cast<int>(test_asset.GetSkinningData().vertices.size());
    entry.scale = 1.0f;

    // Check if ID exists, update or add
    auto it =
        std::find_if(entries.begin(), entries.end(), [&entry](const CharacterEntry& e) { return e.id == entry.id; });
    if (it != entries.end()) {
        *it = entry;
    } else {
        entries.push_back(entry);
    }

    SaveRegistry();
    SelectCharacter(entry.id);
    return true;
}

bool CharacterLibrary::RemoveCharacter(const std::string& id) {
    auto it = std::find_if(entries.begin(), entries.end(), [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries.end())
        return false;

    entries.erase(it);
    SaveRegistry();
    if (active_id == id) {
        active_asset.reset();
        active_id.clear();
        if (!entries.empty()) {
            SelectCharacter(entries.front().id);
        }
    }
    return true;
}

bool CharacterLibrary::SaveMapping(const std::string& id, const CharacterBoneMap& mapping) {
    auto it = std::find_if(entries.begin(), entries.end(), [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries.end())
        return false;
    it->mapping = mapping;
    return true;
}

bool CharacterLibrary::FindEntry(const std::string& id, CharacterEntry& out_entry) const {
    auto it = std::find_if(entries.begin(), entries.end(), [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries.end())
        return false;
    out_entry = *it;
    return true;
}

void CharacterLibrary::Rescan() {
    LoadRegistry();
    RegisterDefaultCharacters();
}

} // namespace studio
