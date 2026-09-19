#include "app/SettingsManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <fstream>
#include <sstream>

namespace studio
{
namespace
{

std::string json_escape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
        }
        out += c;
    }
    return out;
}

bool find_string(const std::string& json, const std::string& key, std::string& out)
{
    const std::string pat = "\"" + key + "\":\"";
    const size_t p = json.find(pat);
    if (p == std::string::npos)
        return false;
    const size_t s = p + pat.size();
    const size_t e = json.find('"', s);
    if (e == std::string::npos)
        return false;
    out = json.substr(s, e - s);
    return true;
}

bool find_int(const std::string& json, const std::string& key, int& out)
{
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos)
        return false;
    try
    {
        out = std::stoi(json.substr(p + pat.size()));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool find_float(const std::string& json, const std::string& key, float& out)
{
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos)
        return false;
    try
    {
        out = std::stof(json.substr(p + pat.size()));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool find_bool(const std::string& json, const std::string& key, bool& out)
{
    const std::string pat = "\"" + key + "\":";
    const size_t p = json.find(pat);
    if (p == std::string::npos)
        return false;
    const std::string rest = json.substr(p + pat.size());
    if (rest.rfind("true", 0) == 0)
    {
        out = true;
        return true;
    }
    if (rest.rfind("false", 0) == 0)
    {
        out = false;
        return true;
    }
    return false;
}

} // namespace

SettingsManager& SettingsManager::GetInstance()
{
    static SettingsManager inst;
    return inst;
}

bool SettingsManager::Load()
{
    file_path = AppPaths::GetSettingsFile();
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        settings.export_dir = AppPaths::DefaultExportDir().string();
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    find_string(json, "theme", settings.theme);
    find_int(json, "target_fps", settings.target_fps);
    find_int(json, "viewportMode", settings.viewport_mode);
    find_bool(json, "showGrid", settings.show_grid);
    find_bool(json, "showAxes", settings.show_axes);
    find_bool(json, "showFloor", settings.show_floor);
    find_bool(json, "showSkeleton", settings.show_skeleton);
    find_bool(json, "showCharacter", settings.show_character);
    find_bool(json, "showWireframe", settings.show_wireframe);
    find_bool(json, "showBoneNames", settings.show_bone_names);
    find_string(json, "selectedCharacterId", settings.selected_character_id);
    find_string(json, "selectedAnimationId", settings.selected_animation_id);
    find_string(json, "exportDir", settings.export_dir);
    find_float(json, "defaultExportFps", settings.default_export_fps);
    find_int(json, "defaultRootMotion", settings.default_root_motion);
    find_bool(json, "playbackLoop", settings.playback_loop);
    find_float(json, "playbackSpeed", settings.playback_speed);

    if (settings.export_dir.empty())
    {
        settings.export_dir = AppPaths::DefaultExportDir().string();
    }

    Logger::GetInstance().Info("Loaded user settings from " + file_path.string());
    return true;
}

bool SettingsManager::save()
{
    AppPaths::EnsureDirectories();
    file_path = AppPaths::GetSettingsFile();
    std::ofstream file(file_path, std::ios::trunc);
    if (!file.is_open())
    {
        Logger::GetInstance().Warning("Failed to open settings file for writing: " + file_path.string());
        return false;
    }

    file << "{\n"
         << "  \"theme\": \"" << json_escape(settings.theme) << "\",\n"
         << "  \"target_fps\": " << settings.target_fps << ",\n"
         << "  \"viewportMode\": " << settings.viewport_mode << ",\n"
         << "  \"showGrid\": " << (settings.show_grid ? "true" : "false") << ",\n"
         << "  \"showAxes\": " << (settings.show_axes ? "true" : "false") << ",\n"
         << "  \"showFloor\": " << (settings.show_floor ? "true" : "false") << ",\n"
         << "  \"showSkeleton\": " << (settings.show_skeleton ? "true" : "false") << ",\n"
         << "  \"showCharacter\": " << (settings.show_character ? "true" : "false") << ",\n"
         << "  \"showWireframe\": " << (settings.show_wireframe ? "true" : "false") << ",\n"
         << "  \"showBoneNames\": " << (settings.show_bone_names ? "true" : "false") << ",\n"
         << "  \"selectedCharacterId\": \"" << json_escape(settings.selected_character_id) << "\",\n"
         << "  \"selectedAnimationId\": \"" << json_escape(settings.selected_animation_id) << "\",\n"
         << "  \"exportDir\": \"" << json_escape(settings.export_dir) << "\",\n"
         << "  \"defaultExportFps\": " << settings.default_export_fps << ",\n"
         << "  \"defaultRootMotion\": " << settings.default_root_motion << ",\n"
         << "  \"playbackLoop\": " << (settings.playback_loop ? "true" : "false") << ",\n"
         << "  \"playbackSpeed\": " << settings.playback_speed << "\n"
         << "}\n";

    Logger::GetInstance().Info("Saved user settings to " + file_path.string());
    return true;
}

} // namespace studio
