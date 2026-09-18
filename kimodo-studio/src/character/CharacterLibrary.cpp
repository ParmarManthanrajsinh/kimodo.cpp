#include "character/CharacterLibrary.h"
#include "character/CharacterLoader.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace studio {
namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

} // namespace

bool FCharacterLibrary::Init() {
    FAppPaths::ensureDirectories();
    LoadRegistry();
    RegisterDefaultCharacters();
    if (!entries.empty()) {
        SelectCharacter(entries.front().id);
    }
    return true;
}

void FCharacterLibrary::RegisterDefaultCharacters() {
    // Check if CesiumMan already registered
    auto it = std::find_if(entries.begin(), entries.end(),
                           [](const FCharacterEntry& e) { return e.id == "cesium-man"; });

    const std::filesystem::path cesiumPath = FAppPaths::resolveAsset("assets/characters/CesiumMan.glb");
    std::error_code ec;
    const std::vector<std::string> dummySource = {
        "Hips", "Spine1", "Spine2", "Chest", "Neck1", "Head",
        "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
        "RightShoulder", "RightArm", "RightForeArm", "RightHand",
        "LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase",
        "RightLeg", "RightShin", "RightFoot", "RightToeBase"
    };

    if (std::filesystem::is_regular_file(cesiumPath, ec) && !ec) {
        if (it == entries.end()) {
            FCharacterAsset temp;
            std::string err;
            if (FCharacterLoader::loadGLB(cesiumPath.string(), temp, err)) {
                FCharacterEntry entry;
                entry.id = "cesium-man";
                entry.name = "Cesium Man (glTF Sample)";
                entry.filePath = cesiumPath.string();
                entry.license = "CC-BY 4.0";
                entry.author = "Cesium (Khronos glTF Sample Assets)";
                entry.boneCount = static_cast<int>(temp.GetBones().size());
                entry.vertexCount = static_cast<int>(temp.GetSkinningData().vertices.size());
                entry.scale = 1.0f;
                entry.mapping = FCharacterMapper::autoMap(temp, dummySource);
                entries.push_back(std::move(entry));
                SaveRegistry();
            }
        } else {
            // Re-evaluate mapping in case alias tables improved
            FCharacterAsset temp;
            std::string err;
            if (FCharacterLoader::loadGLB(cesiumPath.string(), temp, err)) {
                it->mapping = FCharacterMapper::autoMap(temp, dummySource);
                it->boneCount = static_cast<int>(temp.GetBones().size());
                it->vertexCount = static_cast<int>(temp.GetSkinningData().vertices.size());
                SaveRegistry();
            }
        }
    }
}

void FCharacterLibrary::LoadRegistry() {
    entries.clear();
    const auto regPath = FAppPaths::characterRegistryFile();
    std::ifstream file(regPath);
    if (!file.is_open()) return;

    // Minimal JSON array reader
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    size_t pos = 0;
    while ((pos = text.find("{\"id\":\"", pos)) != std::string::npos) {
        FCharacterEntry e;
        size_t idEnd = text.find('"', pos + 7);
        if (idEnd != std::string::npos) {
            e.id = text.substr(pos + 7, idEnd - (pos + 7));
        }

        // Simple property extraction
        auto getProp = [&](const std::string& key) -> std::string {
            std::string pat = "\"" + key + "\":\"";
            size_t p = text.find(pat, pos);
            if (p != std::string::npos && p < text.find("}", pos)) {
                size_t start = p + pat.size();
                size_t end = text.find('"', start);
                if (end != std::string::npos) return text.substr(start, end - start);
            }
            return "";
        };

        e.name = getProp("name");
        e.filePath = getProp("filePath");
        e.license = getProp("license");
        e.author = getProp("author");

        std::error_code ec;
        e.installed = std::filesystem::exists(e.filePath, ec) && !ec;

        if (!e.id.empty()) {
            entries.push_back(e);
        }
        pos = text.find('}', pos);
        if (pos == std::string::npos) break;
        pos++;
    }
}

void FCharacterLibrary::SaveRegistry() {
    const auto regPath = FAppPaths::characterRegistryFile();
    std::ofstream file(regPath, std::ios::trunc);
    if (!file.is_open()) return;

    file << "[\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        file << "  {\n"
             << "    \"id\": \"" << jsonEscape(e.id) << "\",\n"
             << "    \"name\": \"" << jsonEscape(e.name) << "\",\n"
             << "    \"filePath\": \"" << jsonEscape(e.filePath) << "\",\n"
             << "    \"license\": \"" << jsonEscape(e.license) << "\",\n"
             << "    \"author\": \"" << jsonEscape(e.author) << "\",\n"
             << "    \"boneCount\": " << e.boneCount << ",\n"
             << "    \"vertexCount\": " << e.vertexCount << ",\n"
             << "    \"scale\": " << e.scale << "\n"
             << "  }" << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    file << "]\n";
}

bool FCharacterLibrary::SelectCharacter(const std::string& id) {
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&id](const FCharacterEntry& e) { return e.id == id; });
    if (it == entries.end()) return false;

    auto asset = std::make_unique<FCharacterAsset>();
    std::string err;
    if (!FCharacterLoader::loadGLB(it->filePath, *asset, err)) {
        FLogger::GetInstance().error("Failed loading character " + id + ": " + err);
        return false;
    }

    ActiveId = id;
    ActiveAsset = std::move(asset);
    FLogger::GetInstance().info("Selected character: " + it->name + " (" + id + ")");
    return true;
}

bool FCharacterLibrary::ImportCharacter(const std::string& sourcePath, std::string& error) {
    std::filesystem::path src(sourcePath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(src, ec) || ec) {
        error = "File does not exist: " + sourcePath;
        return false;
    }

    // Copy to user characters directory
    const std::filesystem::path dest = FAppPaths::defaultCharactersDir() / src.filename();
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);

    FCharacterAsset testAsset;
    if (!FCharacterLoader::loadGLB(dest.string(), testAsset, error)) {
        return false;
    }

    FCharacterEntry entry;
    entry.id = src.stem().string();
    entry.name = src.stem().string();
    entry.filePath = dest.string();
    entry.license = "User Imported";
    entry.author = "User";
    entry.boneCount = static_cast<int>(testAsset.GetBones().size());
    entry.vertexCount = static_cast<int>(testAsset.GetSkinningData().vertices.size());
    entry.scale = 1.0f;

    // Check if ID exists, update or add
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&entry](const FCharacterEntry& e) { return e.id == entry.id; });
    if (it != entries.end()) {
        *it = entry;
    } else {
        entries.push_back(entry);
    }

    SaveRegistry();
    SelectCharacter(entry.id);
    return true;
}

bool FCharacterLibrary::RemoveCharacter(const std::string& id) {
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&id](const FCharacterEntry& e) { return e.id == id; });
    if (it == entries.end()) return false;

    entries.erase(it);
    SaveRegistry();
    if (ActiveId == id) {
        ActiveAsset.reset();
        ActiveId.clear();
        if (!entries.empty()) {
            SelectCharacter(entries.front().id);
        }
    }
    return true;
}

bool FCharacterLibrary::SaveMapping(const std::string& id, const FCharacterBoneMap& mapping) {
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&id](const FCharacterEntry& e) { return e.id == id; });
    if (it == entries.end()) return false;
    it->mapping = mapping;
    return true;
}

bool FCharacterLibrary::FindEntry(const std::string& id, FCharacterEntry& outEntry) const {
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&id](const FCharacterEntry& e) { return e.id == id; });
    if (it == entries.end()) return false;
    outEntry = *it;
    return true;
}

void FCharacterLibrary::Rescan() {
    LoadRegistry();
    RegisterDefaultCharacters();
}

} // namespace studio
