#pragma once

#include <string>

namespace studio {

enum class Screen {
    Home,
    Generate,
    Models,
    Library,
    Characters,
    Retarget,
    Export,
    Settings,
    Inspector
};

enum class ViewportMode {
    Character = 0,
    Skeleton = 1,
    Both = 2
};

struct AppState {
    Screen screen = Screen::Home;
    Screen lastToolScreen = Screen::Generate;
    std::string gpuName = "RTX GPU";
    int fps = 0;
    bool vulkanAvailable = false;

    // Generation parameters
    std::string prompt = "A person walks forward.";
    int frames = 120;
    int steps = 50;
    unsigned long long seed = 42;
    std::string motionPath;
    std::string textBundle;
    std::string hfUser;

    // Selection state
    std::string retargetSource;    // Library ID selected for retargeting / character preview
    std::string activeCharacterId = "cesium-man"; // Character library ID
    ViewportMode viewportMode = ViewportMode::Both;

    // Viewport display flags
    bool showWireframe = false;
    bool showBoneNames = false;
    bool showGrid = true;
    bool showAxes = true;
    bool showFloor = true;
    bool showSkeleton = true;
    bool showCharacter = true;
};

} // namespace studio
