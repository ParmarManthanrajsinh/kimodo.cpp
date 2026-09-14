#pragma once

#include "app/AppState.h"
#include "kimodo/KimodoEngine.h"
#include "rendering/Viewport.h"
#include "ui/UIManager.h"

namespace studio {

class Application {
public:
    bool init();
    void run();
    void shutdown();

private:
    AppState state_;
    Viewport viewport_;
    UIManager ui_;
    KimodoEngine engine_;
    bool running_ = false;
};

} // namespace studio
