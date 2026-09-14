#pragma once

#include "raylib.h"
#include "rendering/GridRenderer.h"

namespace studio {

class Viewport {
public:
    void reset();
    void frame();
    void update();
    void draw3D() const;
    float distance() const { return dist_; }

private:
    void recomputeCamera() const;

    Vector3 target_ = {0, 1, 0};
    float yaw_ = 0.7f;
    float pitch_ = 0.45f;
    float dist_ = 8.0f;
    mutable Camera3D camera_ = {};
    GridRenderer grid_;
};

} // namespace studio
