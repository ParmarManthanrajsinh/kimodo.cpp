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

namespace studio
{

class Application
{
public:
    bool Init(int width = 1280, int height = 800);
    void Run(int max_frames = 0, const char* screenshot_path = nullptr);
    void Shutdown();

private:
    void PollEngine();
    void UpdateAnimationAndSkinning();

    AppState state;
    Viewport viewport;
    UIManager ui;
    KimodoEngine engine;
    AnimationPlayer player;
    AnimationLibrary library;
    CharacterLibrary characters;
    ModelManager models;
    Toasts toasts;

    EngineStatus last_engine_status = EngineStatus::Idle;
    std::string pendingThumb;
    bool running = false;
};

} // namespace studio
