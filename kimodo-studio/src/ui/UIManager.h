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
class ModelManager;
class Toasts;
class UIManager {
public:
    using CaptureFn = std::function<void(const LibraryEntry&)>;
    void draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
              AnimationPlayer& player, AnimationLibrary& library, ModelManager& models,
              Toasts& toasts, CaptureFn capture = {});
    void drawThumb(const LibraryEntry& e);
    void shutdown(); // joins background auth worker

private:
    std::map<std::string, Texture2D> thumbs_;
    size_t thumbCount_ = 0;
};
} // namespace studio
