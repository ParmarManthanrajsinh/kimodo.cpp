#pragma once

#include "app/AppState.h"

namespace studio {
class FAnimationLibrary;
class FAnimationPlayer;
class SToasts;

class SPageRetarget {
public:
    static void Draw(FAppState& state, FAnimationLibrary& library,
                     FAnimationPlayer& player, SToasts& toasts);
};

} // namespace studio
