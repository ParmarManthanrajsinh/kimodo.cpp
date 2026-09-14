#pragma once

#include "app/AppState.h"

namespace studio {
class Viewport;
class KimodoEngine;
class UIManager {
public:
    void draw(AppState& state, Viewport& viewport, KimodoEngine& engine);
};
} // namespace studio
