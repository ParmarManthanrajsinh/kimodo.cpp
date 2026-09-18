#pragma once

#include "animation/AnimationPlayer.h"
#include "app/AppState.h"

namespace studio {

class STimelineBar {
public:
    static void Draw(FAppState& state, FAnimationPlayer& player, float panelWidth);
};

} // namespace studio
