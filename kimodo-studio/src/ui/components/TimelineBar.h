#pragma once

#include "animation/AnimationPlayer.h"
#include "app/AppState.h"

namespace studio {

class TimelineBar {
public:
    static void Draw(AppState& state, AnimationPlayer& player, float panel_width);
};

} // namespace studio
