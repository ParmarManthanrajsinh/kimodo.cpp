#pragma once

#include "app/AppState.h"

namespace studio {
class FModelManager;
class SToasts;

class SPageModels {
public:
    static void Draw(FAppState& state, FModelManager& models, SToasts& toasts);
};

} // namespace studio
