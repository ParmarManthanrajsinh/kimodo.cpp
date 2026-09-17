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

SettingsManager& SettingsManager::instance() {
    static SettingsManager inst;
    return inst;
}

bool SettingsManager::load() {
    filePath_ = AppPaths::settingsFile();
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        settings_.exportDir = AppPaths::defaultExportDir().string();
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    findString(json, "theme", settings_.theme);
    findInt(json, "targetFps", settings_.targetFps);
    findInt(json, "viewportMode", settings_.viewportMode);
    findBool(json, "showGrid", settings_.showGrid);
    findBool(json, "showAxes", settings_.showAxes);
    findBool(json, "showFloor", settings_.showFloor);
    findBool(json, "showSkeleton", settings_.showSkeleton);
    findBool(json, "showCharacter", settings_.showCharacter);
    findBool(json, "showWireframe", settings_.showWireframe);
    findBool(json, "showBoneNames", settings_.showBoneNames);
    findString(json, "selectedCharacterId", settings_.selectedCharacterId);
    findString(json, "selectedAnimationId", settings_.selectedAnimationId);
    findString(json, "exportDir", settings_.exportDir);
    findFloat(json, "defaultExportFps", settings_.defaultExportFps);
    findInt(json, "defaultRootMotion", settings_.defaultRootMotion);
    findBool(json, "playbackLoop", settings_.playbackLoop);
    findFloat(json, "playbackSpeed", settings_.playbackSpeed);

    if (settings_.exportDir.empty()) {
        settings_.exportDir = AppPaths::defaultExportDir().string();
    }

    Logger::instance().info("Loaded user settings from " + filePath_.string());
    return true;
}

bool SettingsManager::save() {
    AppPaths::ensureDirectories();
    filePath_ = AppPaths::settingsFile();
    std::ofstream file(filePath_, std::ios::trunc);
    if (!file.is_open()) {
        Logger::instance().warning("Failed to open settings file for writing: " + filePath_.string());
        return false;
    }

    file << "{\n"
         << "  \"theme\": \"" << jsonEscape(settings_.theme) << "\",\n"
         << "  \"targetFps\": " << settings_.targetFps << ",\n"
         << "  \"viewportMode\": " << settings_.viewportMode << ",\n"
         << "  \"showGrid\": " << (settings_.showGrid ? "true" : "false") << ",\n"
         << "  \"showAxes\": " << (settings_.showAxes ? "true" : "false") << ",\n"
         << "  \"showFloor\": " << (settings_.showFloor ? "true" : "false") << ",\n"
         << "  \"showSkeleton\": " << (settings_.showSkeleton ? "true" : "false") << ",\n"
         << "  \"showCharacter\": " << (settings_.showCharacter ? "true" : "false") << ",\n"
         << "  \"showWireframe\": " << (settings_.showWireframe ? "true" : "false") << ",\n"
         << "  \"showBoneNames\": " << (settings_.showBoneNames ? "true" : "false") << ",\n"
         << "  \"selectedCharacterId\": \"" << jsonEscape(settings_.selectedCharacterId) << "\",\n"
         << "  \"selectedAnimationId\": \"" << jsonEscape(settings_.selectedAnimationId) << "\",\n"
         << "  \"exportDir\": \"" << jsonEscape(settings_.exportDir) << "\",\n"
         << "  \"defaultExportFps\": " << settings_.defaultExportFps << ",\n"
         << "  \"defaultRootMotion\": " << settings_.defaultRootMotion << ",\n"
         << "  \"playbackLoop\": " << (settings_.playbackLoop ? "true" : "false") << ",\n"
         << "  \"playbackSpeed\": " << settings_.playbackSpeed << "\n"
         << "}\n";

    Logger::instance().info("Saved user settings to " + filePath_.string());
    return true;
}

} // namespace studio
