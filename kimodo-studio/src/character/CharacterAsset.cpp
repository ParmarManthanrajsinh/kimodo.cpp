#include "character/CharacterAsset.h"
#include "raymath.h"

namespace studio
{

CharacterAsset::~CharacterAsset()
{
    Unload();
}

CharacterAsset::CharacterAsset(CharacterAsset&& other) noexcept
    : loaded(other.loaded), Id(std::move(other.Id)), Name(std::move(other.Name)), file_path(std::move(other.file_path)),
      license(std::move(other.license)), author(std::move(other.author)), scale(other.scale), bounds(other.bounds),
      bones(std::move(other.bones)), submeshes(std::move(other.submeshes)),
      skinning_data(std::move(other.skinning_data)), report(std::move(other.report)),
      anim_vertices(std::move(other.anim_vertices)), anim_normals(std::move(other.anim_normals))
{
    other.loaded = false;
}

CharacterAsset& CharacterAsset::operator=(CharacterAsset&& other) noexcept
{
    if (this != &other)
    {
        Unload();
        loaded = other.loaded;
        Id = std::move(other.Id);
        Name = std::move(other.Name);
        file_path = std::move(other.file_path);
        license = std::move(other.license);
        author = std::move(other.author);
        scale = other.scale;
        bounds = other.bounds;
        bones = std::move(other.bones);
        submeshes = std::move(other.submeshes);
        skinning_data = std::move(other.skinning_data);
        report = std::move(other.report);
        anim_vertices = std::move(other.anim_vertices);
        anim_normals = std::move(other.anim_normals);
        other.loaded = false;
    }
    return *this;
}

void CharacterAsset::Unload()
{
    for (auto& sub : submeshes)
    {
        if (sub.has_texture && sub.diffuse_texture.id > 0)
        {
            if (IsWindowReady())
            {
                UnloadTexture(sub.diffuse_texture);
            }
            sub.diffuse_texture.id = 0;
            sub.has_texture = false;
        }
    }
    submeshes.clear();
    bones.clear();
    skinning_data.vertices.clear();
    skinning_data.indices.clear();
    skinning_data.inverse_bind_matrices.clear();
    skinning_data.current_bone_matrices.clear();
    anim_vertices.clear();
    anim_normals.clear();
    loaded = false;
}

int CharacterAsset::FindBoneIndex(const std::string& bone_name) const
{
    for (size_t i = 0; i < bones.size(); ++i)
    {
        if (bones[i].name == bone_name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CharacterAsset::FinalizeGeometry(BoundingBox b)
{
    bounds = b;
    const size_t vcount = skinning_data.vertices.size();
    anim_vertices.resize(vcount);
    anim_normals.resize(vcount);
    for (size_t i = 0; i < vcount; ++i)
    {
        anim_vertices[i] = skinning_data.vertices[i].position;
        anim_normals[i] = skinning_data.vertices[i].normal;
    }
    loaded = !skinning_data.vertices.empty();
}

void CharacterAsset::UpdateCpuSkinning(const std::vector<Matrix>& skin_matrices)
{
    const size_t vcount = skinning_data.vertices.size();
    if (skin_matrices.empty() || anim_vertices.size() != vcount)
    {
        return;
    }

    const int num_bones = static_cast<int>(skin_matrices.size());

    for (size_t i = 0; i < vcount; ++i)
    {
        const SkinVertex& v = skinning_data.vertices[i];
        Vector3 pos_accum{0, 0, 0};
        Vector3 norm_accum{0, 0, 0};
        float total_weight = 0.0f;

        for (int k = 0; k < kMaxInfluences; ++k)
        {
            const float w = v.bone_weights[k];
            if (w <= 1e-4f)
                continue;
            const uint16_t b_idx = v.bone_indices[k];
            if (b_idx >= num_bones)
                continue;

            const Matrix& m = skin_matrices[b_idx];
            Vector3 p = Vector3Transform(v.position, m);
            pos_accum = Vector3Add(pos_accum, Vector3Scale(p, w));

            // Transform normal (rotational part)
            Vector3 norm_rot{m.m0 * v.normal.x + m.m4 * v.normal.y + m.m8 * v.normal.z,
                             m.m1 * v.normal.x + m.m5 * v.normal.y + m.m9 * v.normal.z,
                             m.m2 * v.normal.x + m.m6 * v.normal.y + m.m10 * v.normal.z};
            norm_accum = Vector3Add(norm_accum, Vector3Scale(norm_rot, w));
            total_weight += w;
        }

        if (total_weight > 1e-4f)
        {
            anim_vertices[i] = pos_accum;
            anim_normals[i] = Vector3Normalize(norm_accum);
        }
        else
        {
            anim_vertices[i] = v.position;
            anim_normals[i] = v.normal;
        }
    }
}

} // namespace studio
