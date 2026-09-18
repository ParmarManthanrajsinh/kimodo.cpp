#pragma once

#include "app/AppState.h"

namespace studio {
class FCharacterLibrary;
class FViewport;
class FAnimationPlayer;

class SPageCharacters {
public:
    static void Draw(FAppState& state, FCharacterLibrary& chars, FViewport& viewport,
                     FAnimationPlayer* player = nullptr);
};

} // namespace studio
