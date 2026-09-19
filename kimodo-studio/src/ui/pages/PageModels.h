#pragma once

#include "app/AppState.h"

namespace studio {
class ModelManager;
class Toasts;

class PageModels {
public:
    static void Draw(AppState& state, ModelManager& models, Toasts& toasts);
};

} // namespace studio
