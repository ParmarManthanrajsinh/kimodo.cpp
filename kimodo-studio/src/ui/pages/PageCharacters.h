#pragma once

#include "app/AppState.h"

namespace studio {
class CharacterLibrary;
class Viewport;
class AnimationPlayer;

class PageCharacters {
public:
    static void draw(AppState& state, CharacterLibrary& chars, Viewport& viewport,
                     AnimationPlayer* player = nullptr);
};

} // namespace studio
