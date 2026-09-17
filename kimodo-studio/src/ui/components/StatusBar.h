#pragma once

#include "app/AppState.h"

namespace studio {
class Viewport;

class StatusBar {
public:
    static void draw(AppState& state, Viewport& viewport);
};

} // namespace studio
