#pragma once

#include "app/AppState.h"

namespace studio {
class FAnimationLibrary;
class FCharacterLibrary;

class SPageHome {
public:
    static void Draw(FAppState& state, FAnimationLibrary& lib, FCharacterLibrary& chars);
};

} // namespace studio
