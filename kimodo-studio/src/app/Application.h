#pragma once

#include "animation/AnimationPlayer.h"
#include "app/AppState.h"
#include "character/CharacterLibrary.h"
#include "kimodo/KimodoEngine.h"
#include "library/AnimationLibrary.h"
#include "models/ModelManager.h"
#include "rendering/Viewport.h"
#include "ui/Toast.h"
#include "ui/UIManager.h"

namespace studio {

class Application {
public:
    bool init();
    void run(int maxFrames = 0, const char* screenshotPath = nullptr);
    void shutdown();

private:
    void pollEngine();
    void updateAnimationAndSkinning();

    AppState state_;
    Viewport viewport_;
    UIManager ui_;
    KimodoEngine engine_;
    AnimationPlayer player_;
    AnimationLibrary library_;
    CharacterLibrary characters_;
    ModelManager models_;
    Toasts toasts_;

    EngineStatus lastEngineStatus_ = EngineStatus::Idle;
    std::string pendingThumb_;
    bool running_ = false;
};

} // namespace studio
