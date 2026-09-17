#pragma once

#include "app/AppState.h"

namespace studio {
class Viewport;
class Toasts;

class PageSettings {
public:
    static void draw(AppState& state, Viewport& viewport, Toasts& toasts);
};

} // namespace studio
