#pragma once

#include "app/AppState.h"

namespace studio {
class KimodoEngine;
class ModelManager;

class HeaderBar {
public:
    static void draw(AppState& state, KimodoEngine& engine, ModelManager& models);
};

} // namespace studio
