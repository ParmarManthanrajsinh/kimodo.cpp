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
    viewport_.reset();
    Logger::instance().info("Window + ImGui ready");
    running_ = true;
    return true;
}

void Application::run() {
    while (running_ && !WindowShouldClose()) {
        state_.fps = GetFPS();
        viewport_.update();

        BeginDrawing();
        ClearBackground(Color{18, 18, 22, 255});
        viewport_.draw3D();
        rlImGuiBegin();
        ui_.draw(state_, viewport_, engine_);
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
