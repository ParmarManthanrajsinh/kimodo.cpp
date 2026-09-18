#include "rendering/GridRenderer.h"

#include "raylib.h"

namespace studio {

void FGridRenderer::Draw(bool grid, bool axes) const {
    // Dark studio grid: minor lines subtle, major lines brighter.
    const int half = 20;
    if (grid) {
        for (int i = -half; i <= half; ++i) {
            const bool major = (i % 5 == 0);
            const Color c = major ? Color{52, 52, 60, 255} : Color{32, 32, 38, 255};
            const float f = static_cast<float>(i);
            const float h = static_cast<float>(half);
            DrawLine3D(Vector3{f, 0, -h}, Vector3{f, 0, h}, c);
            DrawLine3D(Vector3{-h, 0, f}, Vector3{h, 0, f}, c);
        }
    }
    // Ground axes through origin.
    if (axes) {
        DrawLine3D(Vector3{-half, 0.01f, 0}, Vector3{half, 0.01f, 0},
                   Color{140, 60, 60, 255});
        DrawLine3D(Vector3{0, 0.01f, -half}, Vector3{0, 0.01f, half},
                   Color{60, 90, 160, 255});
        DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 3, 0}, Color{70, 140, 70, 255});
    }
}

} // namespace studio
