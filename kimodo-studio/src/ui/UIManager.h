#pragma once

#include "app/AppState.h"

namespace studio {
class Viewport;
class KimodoEngine;
class AnimationPlayer;
class UIManager {
public:
    void draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
              AnimationPlayer& player);
};
} // namespace studio
