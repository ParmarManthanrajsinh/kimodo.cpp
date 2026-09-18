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

class FApplication {
public:
    bool Init(int width = 1280, int height = 800);
    void Run(int maxFrames = 0, const char* screenshotPath = nullptr);
    void Shutdown();

private:
    void PollEngine();
    void UpdateAnimationAndSkinning();

    FAppState state;
    FViewport viewport;
    FUIManager ui;
    FKimodoEngine engine;
    FAnimationPlayer player;
    FAnimationLibrary library;
    FCharacterLibrary characters;
    FModelManager models;
    SToasts toasts;

    EEngineStatus lastEngineStatus = EEngineStatus::Idle;
    std::string pendingThumb;
    bool bRunning = false;
};

} // namespace studio
