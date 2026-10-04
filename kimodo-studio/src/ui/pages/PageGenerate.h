#pragma once

#include "app/AppState.h"

namespace studio
{
class KimodoEngine;
class ModelManager;
class SetupManager;
class Toasts;

class PageGenerate
{
public:
    static void Draw(AppState& state, KimodoEngine& engine, ModelManager& models, SetupManager& setup, Toasts& toasts);
};

} // namespace studio
