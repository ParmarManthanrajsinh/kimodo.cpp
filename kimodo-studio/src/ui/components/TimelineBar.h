#pragma once

#include "animation/AnimationPlayer.h"
#include "app/AppState.h"

namespace studio {

class TimelineBar {
public:
    static void draw(AppState& state, AnimationPlayer& player, float panelWidth);
};

} // namespace studio
