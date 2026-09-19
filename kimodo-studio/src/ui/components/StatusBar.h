#pragma once

#include "app/AppState.h"

namespace studio
{
class Viewport;

class StatusBar
{
public:
    static void Draw(AppState& state, Viewport& viewport);
};

} // namespace studio
