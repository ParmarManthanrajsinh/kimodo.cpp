#pragma once

#include "app/AppState.h"

namespace studio {
class FKimodoEngine;
class FModelManager;

class SHeaderBar {
public:
    static void Draw(FAppState& state, FKimodoEngine& engine, FModelManager& models);
};

} // namespace studio
