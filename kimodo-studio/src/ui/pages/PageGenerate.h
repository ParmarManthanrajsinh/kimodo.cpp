#pragma once

#include "app/AppState.h"

namespace studio {
class FKimodoEngine;
class FModelManager;
class SToasts;

class SPageGenerate {
public:
    static void Draw(FAppState& state, FKimodoEngine& engine, FModelManager& models, SToasts& toasts);
};

} // namespace studio
