#include "rendering/SkinningRenderer.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace studio {

FSkinningRenderer::~FSkinningRenderer() {
    Shutdown();
}

bool FSkinningRenderer::Init() {
    bInitialized = true;
    return true;
}

void FSkinningRenderer::Shutdown() {
    if (bInitialized) {
        bInitialized = false;
    }
}

void FSkinningRenderer::drawCharacter(FCharacterAsset& character,
                                     const std::vector<Matrix>& skinMatrices,
                                     bool wireframe) {
    if (!character.IsLoaded() || character.GetSkinningData().vertices.empty()) {
        return;
    }

    // Update skinning matrices
    character.updateCpuSkinning(skinMatrices);

    const auto& animVerts = character.GetAnimatedVertices();
    const auto& animNorms = character.GetAnimatedNormals();
    const auto& origVerts = character.GetSkinningData().vertices;
    const auto& indices = character.GetSkinningData().indices;
    const auto& submeshes = character.GetSubmeshes();

    // Default character material colors
    const Color defaultSkinColor = Color{210, 215, 225, 255};
    const Color wireColor = Color{56, 168, 232, 255};

    if (wireframe) {
        rlEnableWireMode();
    }

    // Render each submesh
    for (const auto& sub : submeshes) {
        if (sub.indexCount == 0) continue;

        if (sub.hasTexture && sub.diffuseTexture.id > 0) {
            rlSetTexture(sub.diffuseTexture.id);
        } else {
            rlSetTexture(0);
        }

        rlBegin(RL_TRIANGLES);

        const uint32_t endIdx = sub.indexOffset + sub.indexCount;
        for (uint32_t i = sub.indexOffset; i < endIdx; ++i) {
            if (i >= indices.size()) break;
            const uint32_t vIdx = indices[i];
            if (vIdx >= animVerts.size()) break;

            const Vector3& pos = animVerts[vIdx];
            const Vector3& norm = animNorms[vIdx];
            const Vector2& uv = origVerts[vIdx].texcoord;

            // Simple directional key/fill light shading
            Vector3 lightDir = Vector3Normalize({0.5f, 1.0f, 0.7f});
            float diff = std::max(0.2f, Vector3DotProduct(norm, lightDir));
            float fill = std::max(0.1f, Vector3DotProduct(norm, Vector3{-0.5f, 0.2f, -0.7f})) * 0.3f;
            float light = std::min(1.0f, diff + fill);

            Color c = wireframe ? wireColor : (sub.hasTexture ? WHITE : sub.baseColor);
            c.r = static_cast<unsigned char>(c.r * light);
            c.g = static_cast<unsigned char>(c.g * light);
            c.b = static_cast<unsigned char>(c.b * light);

            rlColor4ub(c.r, c.g, c.b, c.a);
            rlTexCoord2f(uv.x, uv.y);
            rlNormal3f(norm.x, norm.y, norm.z);
            rlVertex3f(pos.x, pos.y, pos.z);
        }

        rlEnd();
    }

    rlSetTexture(0);

    if (wireframe) {
        rlDisableWireMode();
    }
}

} // namespace studio
