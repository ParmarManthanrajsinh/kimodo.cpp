#pragma once

#include <functional>
#include <map>
#include <string>
#include "app/AppState.h"
#include "raylib.h"

namespace studio {

class Viewport;
class KimodoEngine;
class AnimationPlayer;
class AnimationLibrary;
struct LibraryEntry;
class CharacterLibrary;
class ModelManager;
class Toasts;

class UIManager {
public:
    using CaptureFn = std::function<void(const LibraryEntry&)>;

    void Draw(AppState& state, Viewport& viewport, KimodoEngine& engine, AnimationPlayer& player,
              AnimationLibrary& library, CharacterLibrary& characters, ModelManager& models, Toasts& toasts,
              CaptureFn capture = {});

    void Shutdown();

private:
    std::map<std::string, Texture2D> thumbs;
};

} // namespace studio
