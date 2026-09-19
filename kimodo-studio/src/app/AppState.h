#pragma once

#include <string>
#include "raylib.h"

namespace studio
{

enum class Screen
{
    Home,
    Generate,
    Models,
    Library,
    Characters,
    Retarget,
    Export,
    settings,
    Inspector
};

enum class ViewportMode
{
    Character = 0,
    Skeleton = 1,
    Both = 2
};

struct AppState
{
    Screen screen = Screen::Home;
    Screen last_tool_screen = Screen::Generate;
    std::string gpu_name = "RTX GPU";
    int fps = 0;
    bool vulkan_available = false;

    // Generation parameters
    std::string prompt = "A person walks forward.";
    int frames = 120;
    int steps = 50;
    unsigned long long seed = 42;
    std::string motion_path;
    std::string text_bundle;
    std::string hfUser;

    // Selection state
    std::string retarget_source;                    // Library ID selected for retargeting / character preview
    std::string active_character_id = "cesium-man"; // Character library ID
    ViewportMode viewport_mode = ViewportMode::Both;

    // Viewport display flags
    bool show_wireframe = false;
    bool show_bone_names = false;
    bool show_joint_names = false;
    bool show_grid = true;
    bool show_axes = true;
    bool show_floor = true;
    bool show_skeleton = true;
    bool show_character = true;

    // Camera and Viewport
    int camera_projection = 0; // 0 = Perspective, 1 = Orthographic

    // Model Transform (XYZ)
    Vector3 model_position = {0.0f, 0.0f, 0.0f};
    Vector3 model_rotation = {0.0f, 0.0f, 0.0f};
    Vector3 model_scale = {1.0f, 1.0f, 1.0f};

    // Playback control
    float playback_speed = 1.0f;
    int target_fps = 30;

    // Right Workspace Panel tabs: 0 = Animation, 1 = Character, 2 = Inspector
    int right_panel_tab = 1;

    // Resizable UI panel dimensions
    float side_width = 168.0f;
    float panel_width = 320.0f;
};

} // namespace studio
