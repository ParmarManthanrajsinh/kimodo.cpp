#include "rendering/GridRenderer.h"

#include "raylib.h"

namespace studio {

void GridRenderer::draw() const {
    DrawGrid(20, 1.0f);
    DrawPlane(Vector3{0, -0.01f, 0}, Vector2{40, 40}, Color{24, 24, 30, 255});
    DrawLine3D(Vector3{0, 0, 0}, Vector3{3, 0, 0}, RED);
    DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 3, 0}, GREEN);
    DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 0, 3}, BLUE);
}

} // namespace studio
