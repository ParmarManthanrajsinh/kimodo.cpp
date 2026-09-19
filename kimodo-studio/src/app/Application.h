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

#include <atomic>
#include <thread>

struct GLFWwindow;

namespace studio
{

class Application
{
public:
    bool Init(int width = 1280, int height = 800);
    void Run(int max_frames = 0, const char* screenshot_path = nullptr);
    void RenderFrame();
    void Shutdown();

private:
    void RenderLoop();
    void PollEngine();
    void UpdateAnimationAndSkinning(float dt);

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

    // Decoupled Multi-threaded Hybrid Architecture
    GLFWwindow* glfw_window = nullptr;
    std::thread render_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> render_finished{false};
    double last_frame_time = 0.0;
};

} // namespace studio
