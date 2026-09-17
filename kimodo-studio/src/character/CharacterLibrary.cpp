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

bool CharacterLibrary::init() {
    AppPaths::ensureDirectories();
    loadRegistry();
    registerDefaultCharacters();
    if (!entries_.empty()) {
        selectCharacter(entries_.front().id);
    }
    return true;
}

void CharacterLibrary::registerDefaultCharacters() {
    // Check if CesiumMan already registered
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [](const CharacterEntry& e) { return e.id == "cesium-man"; });

    const std::filesystem::path cesiumPath = AppPaths::resolveAsset("assets/characters/CesiumMan.glb");
    std::error_code ec;
    if (std::filesystem::is_regular_file(cesiumPath, ec) && !ec) {
        if (it == entries_.end()) {
            CharacterAsset temp;
            std::string err;
            if (CharacterLoader::loadGLB(cesiumPath.string(), temp, err)) {
                CharacterEntry entry;
                entry.id = "cesium-man";
                entry.name = "Cesium Man (glTF Sample)";
                entry.filePath = cesiumPath.string();
                entry.license = "CC-BY 4.0";
                entry.author = "Cesium (Khronos glTF Sample Assets)";
                entry.boneCount = static_cast<int>(temp.bones().size());
                entry.vertexCount = static_cast<int>(temp.skinningData().vertices.size());
                entry.scale = 1.0f;
                // Auto-map with default SOMA joints
                const std::vector<std::string> dummySource = {
                    "Hips", "Spine1", "Spine2", "Chest", "Neck1", "Head",
                    "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
                    "RightShoulder", "RightArm", "RightForeArm", "RightHand",
                    "LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase",
                    "RightLeg", "RightShin", "RightFoot", "RightToeBase"
                };
                entry.mapping = CharacterMapper::autoMap(temp, dummySource);
                entries_.push_back(std::move(entry));
                saveRegistry();
            }
        }
    }
}

void CharacterLibrary::loadRegistry() {
    entries_.clear();
    const auto regPath = AppPaths::characterRegistryFile();
    std::ifstream file(regPath);
    if (!file.is_open()) return;

    // Minimal JSON array reader
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    size_t pos = 0;
    while ((pos = text.find("{\"id\":\"", pos)) != std::string::npos) {
        CharacterEntry e;
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
            entries_.push_back(e);
        }
        pos = text.find('}', pos);
        if (pos == std::string::npos) break;
        pos++;
    }
}

void CharacterLibrary::saveRegistry() {
    const auto regPath = AppPaths::characterRegistryFile();
    std::ofstream file(regPath, std::ios::trunc);
    if (!file.is_open()) return;

    file << "[\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        file << "  {\n"
             << "    \"id\": \"" << jsonEscape(e.id) << "\",\n"
             << "    \"name\": \"" << jsonEscape(e.name) << "\",\n"
             << "    \"filePath\": \"" << jsonEscape(e.filePath) << "\",\n"
             << "    \"license\": \"" << jsonEscape(e.license) << "\",\n"
             << "    \"author\": \"" << jsonEscape(e.author) << "\",\n"
             << "    \"boneCount\": " << e.boneCount << ",\n"
             << "    \"vertexCount\": " << e.vertexCount << ",\n"
             << "    \"scale\": " << e.scale << "\n"
             << "  }" << (i + 1 < entries_.size() ? "," : "") << "\n";
    }
    file << "]\n";
}

bool CharacterLibrary::selectCharacter(const std::string& id) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries_.end()) return false;

    auto asset = std::make_unique<CharacterAsset>();
    std::string err;
    if (!CharacterLoader::loadGLB(it->filePath, *asset, err)) {
        Logger::instance().error("Failed loading character " + id + ": " + err);
        return false;
    }

    activeId_ = id;
    activeAsset_ = std::move(asset);
    Logger::instance().info("Selected character: " + it->name + " (" + id + ")");
    return true;
}

bool CharacterLibrary::importCharacter(const std::string& sourcePath, std::string& error) {
    std::filesystem::path src(sourcePath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(src, ec) || ec) {
        error = "File does not exist: " + sourcePath;
        return false;
    }

    // Copy to user characters directory
    const std::filesystem::path dest = AppPaths::defaultCharactersDir() / src.filename();
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);

    CharacterAsset testAsset;
    if (!CharacterLoader::loadGLB(dest.string(), testAsset, error)) {
        return false;
    }

    CharacterEntry entry;
    entry.id = src.stem().string();
    entry.name = src.stem().string();
    entry.filePath = dest.string();
    entry.license = "User Imported";
    entry.author = "User";
    entry.boneCount = static_cast<int>(testAsset.bones().size());
    entry.vertexCount = static_cast<int>(testAsset.skinningData().vertices.size());
    entry.scale = 1.0f;

    // Check if ID exists, update or add
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&entry](const CharacterEntry& e) { return e.id == entry.id; });
    if (it != entries_.end()) {
        *it = entry;
    } else {
        entries_.push_back(entry);
    }

    saveRegistry();
    selectCharacter(entry.id);
    return true;
}

bool CharacterLibrary::removeCharacter(const std::string& id) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries_.end()) return false;

    entries_.erase(it);
    saveRegistry();
    if (activeId_ == id) {
        activeAsset_.reset();
        activeId_.clear();
        if (!entries_.empty()) {
            selectCharacter(entries_.front().id);
        }
    }
    return true;
}

bool CharacterLibrary::saveMapping(const std::string& id, const CharacterBoneMap& mapping) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries_.end()) return false;
    it->mapping = mapping;
    return true;
}

bool CharacterLibrary::findEntry(const std::string& id, CharacterEntry& outEntry) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const CharacterEntry& e) { return e.id == id; });
    if (it == entries_.end()) return false;
    outEntry = *it;
    return true;
}

void CharacterLibrary::rescan() {
    loadRegistry();
    registerDefaultCharacters();
}

} // namespace studio
