#pragma once

#include "app/AppState.h"

namespace studio {
class AnimationPlayer;
class AnimationLibrary;
class CharacterLibrary;
class Toasts;

class PageExport {
public:
    static void draw(AppState& state, AnimationPlayer& player,
                     AnimationLibrary& library, CharacterLibrary& chars,
                     Toasts& toasts);
};

} // namespace studio
