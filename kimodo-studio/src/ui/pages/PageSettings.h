#pragma once

#include "app/AppState.h"

namespace studio {
class FViewport;
class SToasts;

class SPageSettings {
public:
    static void Draw(FAppState& state, FViewport& viewport, SToasts& toasts);
};

} // namespace studio
