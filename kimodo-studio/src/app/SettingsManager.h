#pragma once

#include <filesystem>
#include <string>

namespace studio
{

struct UserSettings
{
    std::string theme = "Dark";
    int target_fps = 60;
    int viewport_mode = 2; // 0 = Character, 1 = Skeleton, 2 = Both
    bool show_grid = true;
    bool show_axes = true;
    bool show_floor = true;
    bool show_skeleton = true;
    bool show_character = true;
    bool show_wireframe = false;
    bool show_bone_names = false;

    std::string selected_character_id = "cesium-man";
    std::string selected_animation_id;
    std::string export_dir;
    float default_export_fps = 30.0f;
    int default_root_motion = 0; // 0 = Preserve, 1 = LockX, 2 = LockXZ, 3 = Zero
    bool playback_loop = true;
    float playback_speed = 1.0f;
};

class SettingsManager
{
public:
    static SettingsManager& GetInstance();

    bool Load();
    bool save();

    UserSettings& GetSettings()
    {
        return settings;
    }
    const UserSettings& GetSettings() const
    {
        return settings;
    }

private:
    SettingsManager() = default;
    UserSettings settings;
    std::filesystem::path file_path;
};

} // namespace studio
