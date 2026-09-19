#pragma once

#include "app/AppState.h"

namespace studio {
class KimodoEngine;
class ModelManager;
class Toasts;

class PageGenerate {
public:
    static void Draw(AppState& state, KimodoEngine& engine, ModelManager& models, Toasts& toasts);
};

} // namespace studio
