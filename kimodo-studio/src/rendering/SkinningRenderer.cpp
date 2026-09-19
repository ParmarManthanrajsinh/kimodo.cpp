#include "rendering/SkinningRenderer.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace studio
{

SkinningRenderer::~SkinningRenderer()
{
    Shutdown();
}

bool SkinningRenderer::Init()
{
    initialized = true;
    return true;
}

void SkinningRenderer::Shutdown()
{
    if (initialized)
    {
        initialized = false;
    }
}

void SkinningRenderer::DrawCharacter(CharacterAsset& character, const std::vector<Matrix>& skin_matrices,
                                     bool wireframe)
{
    if (!character.IsLoaded() || character.GetSkinningData().vertices.empty())
    {
        return;
    }

    // Update skinning matrices
    character.UpdateCpuSkinning(skin_matrices);

    const auto& anim_verts = character.GetAnimatedVertices();
    const auto& anim_norms = character.GetAnimatedNormals();
    const auto& orig_verts = character.GetSkinningData().vertices;
    const auto& indices = character.GetSkinningData().indices;
    const auto& submeshes = character.GetSubmeshes();

    // Default character material colors
    const Color default_skin_color = Color{210, 215, 225, 255};
    const Color wire_color = Color{56, 168, 232, 255};

    if (wireframe)
    {
        rlEnableWireMode();
    }

    // Render each submesh
    for (const auto& sub : submeshes)
    {
        if (sub.index_count == 0)
            continue;

        if (sub.has_texture && sub.diffuse_texture.id > 0)
        {
            rlSetTexture(sub.diffuse_texture.id);
        }
        else
        {
            rlSetTexture(0);
        }

        rlBegin(RL_TRIANGLES);

        const uint32_t end_idx = sub.index_offset + sub.index_count;
        for (uint32_t i = sub.index_offset; i < end_idx; ++i)
        {
            if (i >= indices.size())
                break;
            const uint32_t v_idx = indices[i];
            if (v_idx >= anim_verts.size())
                break;

            const Vector3& pos = anim_verts[v_idx];
            const Vector3& norm = anim_norms[v_idx];
            const Vector2& uv = orig_verts[v_idx].texcoord;

            // Simple directional key/fill light shading
            Vector3 light_dir = Vector3Normalize({0.5f, 1.0f, 0.7f});
            float diff = std::max(0.2f, Vector3DotProduct(norm, light_dir));
            float fill = std::max(0.1f, Vector3DotProduct(norm, Vector3{-0.5f, 0.2f, -0.7f})) * 0.3f;
            float light = std::min(1.0f, diff + fill);

            Color c = wireframe ? wire_color : (sub.has_texture ? WHITE : sub.base_color);
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

    if (wireframe)
    {
        rlDisableWireMode();
    }
}

} // namespace studio
