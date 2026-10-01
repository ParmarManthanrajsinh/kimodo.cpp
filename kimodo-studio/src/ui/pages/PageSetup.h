#pragma once

#include "app/AppState.h"
#include "app/SetupManager.h"
#include "models/ModelManager.h"
#include "ui/Toast.h"

namespace studio
{

class PageSetup
{
public:
    static void Draw(AppState& state, SetupManager& setup, ModelManager& models, Toasts& toasts);
};

} // namespace studio
