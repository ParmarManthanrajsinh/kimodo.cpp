#pragma once

#include "app/AppState.h"

namespace studio
{
class Viewport;
class Toasts;

class PageSettings
{
public:
    static void Draw(AppState& state, Viewport& viewport, Toasts& toasts);
};

} // namespace studio
