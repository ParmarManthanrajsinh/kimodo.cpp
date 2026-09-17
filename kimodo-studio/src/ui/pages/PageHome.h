#pragma once

#include "app/AppState.h"

namespace studio {
class AnimationLibrary;
class CharacterLibrary;

class PageHome {
public:
    static void draw(AppState& state, AnimationLibrary& lib, CharacterLibrary& chars);
};

} // namespace studio
