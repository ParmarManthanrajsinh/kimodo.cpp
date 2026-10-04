#pragma once

#include "app/AppState.h"
#include "app/SetupManager.h"
#include "models/ModelManager.h"

namespace studio
{
class Viewport;
class Toasts;

class PageSettings
{
public:
    static void Draw(AppState& state, Viewport& viewport, SetupManager& setup, ModelManager& models, Toasts& toasts);
};

} // namespace studio
