#pragma once

#include <filesystem>
#include <string>

namespace studio {

struct FUserSettings {
    std::string theme = "Dark";
    int targetFps = 60;
    int viewportMode = 2; // 0 = Character, 1 = Skeleton, 2 = Both
    bool showGrid = true;
    bool showAxes = true;
    bool showFloor = true;
    bool showSkeleton = true;
    bool showCharacter = true;
    bool showWireframe = false;
    bool showBoneNames = false;

    std::string selectedCharacterId = "cesium-man";
    std::string selectedAnimationId;
    std::string exportDir;
    float defaultExportFps = 30.0f;
    int defaultRootMotion = 0; // 0 = Preserve, 1 = LockX, 2 = LockXZ, 3 = Zero
    bool playbackLoop = true;
    float playbackSpeed = 1.0f;
};

class FSettingsManager {
public:
    static FSettingsManager& GetInstance();

    bool load();
    bool save();

    FUserSettings& GetSettings() { return Settings; }
    const FUserSettings& GetSettings() const { return Settings; }

private:
    FSettingsManager() = default;
    FUserSettings Settings;
    std::filesystem::path filePath;
};

} // namespace studio
