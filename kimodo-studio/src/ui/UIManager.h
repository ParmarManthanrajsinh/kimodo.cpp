#pragma once

#include "app/AppState.h"

namespace studio {
class Viewport;
class KimodoEngine;
class AnimationPlayer;
class AnimationLibrary;
class ModelManager;
class Toasts;
class UIManager {
public:
    void draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
              AnimationPlayer& player, AnimationLibrary& library, ModelManager& models,
              Toasts& toasts);
};
} // namespace studio
