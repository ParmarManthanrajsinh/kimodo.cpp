#pragma once

#include "app/AppState.h"

namespace studio {
class AnimationLibrary;
class AnimationPlayer;
class Toasts;

class PageRetarget {
public:
    static void draw(AppState& state, AnimationLibrary& library,
                     AnimationPlayer& player, Toasts& toasts);
};

} // namespace studio
