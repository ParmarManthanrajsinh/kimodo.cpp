#pragma once

#include "raylib.h"
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
    bool showJointNames = false;
    bool showGrid = true;
    bool showAxes = true;
    bool showFloor = true;
    bool showSkeleton = true;
    bool showCharacter = true;

    // Camera and Viewport
    int cameraProjection = 0;      // 0 = Perspective, 1 = Orthographic

    // Model Transform (XYZ)
    Vector3 modelPosition = {0.0f, 0.0f, 0.0f};
    Vector3 modelRotation = {0.0f, 0.0f, 0.0f};
    Vector3 modelScale = {1.0f, 1.0f, 1.0f};

    // Playback control
    float playbackSpeed = 1.0f;
    int targetFps = 30;

    // Right Workspace Panel tabs: 0 = Animation, 1 = Character, 2 = Inspector
    int rightPanelTab = 1;

    // Resizable UI panel dimensions
    float sideWidth = 168.0f;
    float panelWidth = 320.0f;
};

} // namespace studio
