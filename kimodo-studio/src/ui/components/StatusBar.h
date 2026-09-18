#pragma once

#include "app/AppState.h"

namespace studio {
class FViewport;

class SStatusBar {
public:
    static void Draw(FAppState& state, FViewport& viewport);
};

} // namespace studio
