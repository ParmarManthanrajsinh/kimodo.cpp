#pragma once

#include "app/AppState.h"
#include "raylib.h"
#include <functional>
#include <map>
#include <string>

namespace studio {

class FViewport;
class FKimodoEngine;
class FAnimationPlayer;
class FAnimationLibrary;
struct FLibraryEntry;
class FCharacterLibrary;
class FModelManager;
class SToasts;

class FUIManager {
public:
    using CaptureFn = std::function<void(const FLibraryEntry&)>;

    void Draw(FAppState& state, FViewport& viewport, FKimodoEngine& engine,
              FAnimationPlayer& player, FAnimationLibrary& library,
              FCharacterLibrary& characters, FModelManager& models,
              SToasts& toasts, CaptureFn capture = {});

    void Shutdown();

private:
    std::map<std::string, Texture2D> thumbs;
};

} // namespace studio
