#pragma once

#include "animation/AnimationPlayer.h"
#include "app/AppState.h"
#include "kimodo/KimodoEngine.h"
#include "library/AnimationLibrary.h"
#include "rendering/Viewport.h"
#include "ui/Toast.h"
#include "ui/UIManager.h"

namespace studio {

class Application {
public:
    bool init();
    void run();
    void shutdown();

private:
    void pollEngine();

    AppState state_;
    Viewport viewport_;
    UIManager ui_;
    KimodoEngine engine_;
    AnimationPlayer player_;
    AnimationLibrary library_;
    Toasts toasts_;
    EngineStatus lastEngineStatus_ = EngineStatus::Idle;
    bool running_ = false;
};

} // namespace studio
