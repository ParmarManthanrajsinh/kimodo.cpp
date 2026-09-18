#pragma once

#include "app/AppState.h"

namespace studio {
class FAnimationPlayer;
class FAnimationLibrary;
class FCharacterLibrary;
class SToasts;

class SPageExport {
public:
    static void Draw(FAppState& state, FAnimationPlayer& player,
                     FAnimationLibrary& library, FCharacterLibrary& chars,
                     SToasts& toasts);
};

} // namespace studio
