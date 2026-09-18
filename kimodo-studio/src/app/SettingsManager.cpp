#include "app/SettingsManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <fstream>
#include <sstream>

namespace studio {
namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

bool findString(const std::string& json, const std::string& key, std::string& out) {
    const std::string pat = "\"" + key + "\":\"";
    const size_t p = json.find(pat);
    if (p == std::string::npos) return false;
    const size_t s = p + pat.size();
    const size_t e = json.find('"', s);
    if (e == std::string::npos) return false;
    out = json.substr(s, e - s);
    return true;
}

bool findInt(const std::string& json, const std::string& key, int& out) {
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos) return false;
    try {
        out = std::stoi(json.substr(p + pat.size()));
        return true;
    } catch (...) {
        return false;
    }
}

bool findFloat(const std::string& json, const std::string& key, float& out) {
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos) return false;
    try {
        out = std::stof(json.substr(p + pat.size()));
        return true;
    } catch (...) {
        return false;
    }
}

bool findBool(const std::string& json, const std::string& key, bool& out) {
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos) return false;
    const std::string rest = json.substr(p + pat.size());
    if (rest.rfind("true", 0) == 0) {
        out = true;
        return true;
    }
    if (rest.rfind("false", 0) == 0) {
        out = false;
        return true;
    }
    return false;
}

} // namespace

FSettingsManager& FSettingsManager::GetInstance() {
    static FSettingsManager inst;
    return inst;
}

bool FSettingsManager::load() {
    filePath = FAppPaths::GetSettingsFile();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Settings.exportDir = FAppPaths::defaultExportDir().string();
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    findString(json, "theme", Settings.theme);
    findInt(json, "targetFps", Settings.targetFps);
    findInt(json, "viewportMode", Settings.viewportMode);
    findBool(json, "showGrid", Settings.showGrid);
    findBool(json, "showAxes", Settings.showAxes);
    findBool(json, "showFloor", Settings.showFloor);
    findBool(json, "showSkeleton", Settings.showSkeleton);
    findBool(json, "showCharacter", Settings.showCharacter);
    findBool(json, "showWireframe", Settings.showWireframe);
    findBool(json, "showBoneNames", Settings.showBoneNames);
    findString(json, "selectedCharacterId", Settings.selectedCharacterId);
    findString(json, "selectedAnimationId", Settings.selectedAnimationId);
    findString(json, "exportDir", Settings.exportDir);
    findFloat(json, "defaultExportFps", Settings.defaultExportFps);
    findInt(json, "defaultRootMotion", Settings.defaultRootMotion);
    findBool(json, "playbackLoop", Settings.playbackLoop);
    findFloat(json, "playbackSpeed", Settings.playbackSpeed);

    if (Settings.exportDir.empty()) {
        Settings.exportDir = FAppPaths::defaultExportDir().string();
    }

    FLogger::GetInstance().info("Loaded user settings from " + filePath.string());
    return true;
}

bool FSettingsManager::save() {
    FAppPaths::ensureDirectories();
    filePath = FAppPaths::GetSettingsFile();
    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open()) {
        FLogger::GetInstance().warning("Failed to open settings file for writing: " + filePath.string());
        return false;
    }

    file << "{\n"
         << "  \"theme\": \"" << jsonEscape(Settings.theme) << "\",\n"
         << "  \"targetFps\": " << Settings.targetFps << ",\n"
         << "  \"viewportMode\": " << Settings.viewportMode << ",\n"
         << "  \"showGrid\": " << (Settings.showGrid ? "true" : "false") << ",\n"
         << "  \"showAxes\": " << (Settings.showAxes ? "true" : "false") << ",\n"
         << "  \"showFloor\": " << (Settings.showFloor ? "true" : "false") << ",\n"
         << "  \"showSkeleton\": " << (Settings.showSkeleton ? "true" : "false") << ",\n"
         << "  \"showCharacter\": " << (Settings.showCharacter ? "true" : "false") << ",\n"
         << "  \"showWireframe\": " << (Settings.showWireframe ? "true" : "false") << ",\n"
         << "  \"showBoneNames\": " << (Settings.showBoneNames ? "true" : "false") << ",\n"
         << "  \"selectedCharacterId\": \"" << jsonEscape(Settings.selectedCharacterId) << "\",\n"
         << "  \"selectedAnimationId\": \"" << jsonEscape(Settings.selectedAnimationId) << "\",\n"
         << "  \"exportDir\": \"" << jsonEscape(Settings.exportDir) << "\",\n"
         << "  \"defaultExportFps\": " << Settings.defaultExportFps << ",\n"
         << "  \"defaultRootMotion\": " << Settings.defaultRootMotion << ",\n"
         << "  \"playbackLoop\": " << (Settings.playbackLoop ? "true" : "false") << ",\n"
         << "  \"playbackSpeed\": " << Settings.playbackSpeed << "\n"
         << "}\n";

    FLogger::GetInstance().info("Saved user settings to " + filePath.string());
    return true;
}

} // namespace studio
