#include "app/Application.h"

#include "raylib.h"
#include "rlImGui.h"
#include "ui/Theme.h"
#include "utils/Logger.h"

namespace studio {

bool Application::init() {
    Logger::instance().init(Logger::defaultLogFile());
    Logger::instance().info("Kimodo Studio starting");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800, "Kimodo Studio");
    if (!IsWindowReady()) {
        Logger::instance().error("Raylib window init failed");
        return false;
    }
    SetTargetFPS(60);

    rlImGuiSetup(true);
    Theme::apply();

    state_.gpuName = "default GPU";
    engine_.setPaths(state_.motionPath, state_.textBundle);
    library_.init(AnimationLibrary::defaultBaseDir());
    viewport_.reset();
    Logger::instance().info("Window + ImGui ready");
    running_ = true;
    return true;
}

void Application::pollEngine() {
    EngineStatus s = engine_.status();
    if (s == EngineStatus::Finished && s != lastEngineStatus_) {
        MotionResult result;
        if (engine_.lastResult(result)) {
            Animation anim;
            anim.fromMotionResult(result);
            player_.load(anim);
            Logger::instance().info("Viewport: animation loaded, " +
                                    std::to_string(anim.frames) + " frames");
            LibraryEntry saved;
            if (library_.save(engine_.lastPrompt(), "soma-rp-v1.1", anim.fps, result,
                              saved)) {
                toasts_.push("Animation saved to library", ToastKind::Success);
            } else {
                toasts_.push("Animation generated, library save failed",
                             ToastKind::Warning);
            }
        }
    }
    if (s == EngineStatus::Error && s != lastEngineStatus_) {
        toasts_.push(engine_.message(), ToastKind::Error);
    }
    lastEngineStatus_ = s;
}

void Application::run() {
    while (running_ && !WindowShouldClose()) {
        state_.fps = GetFPS();
        pollEngine();
        player_.update(GetFrameTime());
        if (player_.hasAnimation()) {
            viewport_.setPose(player_.worldPositions());
        }
        viewport_.update();

        BeginDrawing();
        ClearBackground(Color{18, 18, 22, 255});
        viewport_.draw3D();
        rlImGuiBegin();
        ui_.draw(state_, viewport_, engine_, player_, library_, toasts_);
        toasts_.draw();
        rlImGuiEnd();
        EndDrawing();
    }
}

void Application::shutdown() {
    engine_.shutdown();
    rlImGuiShutdown();
    if (IsWindowReady()) {
        CloseWindow();
    }
    Logger::instance().info("Kimodo Studio shutdown clean");
    running_ = false;
}

} // namespace studio
