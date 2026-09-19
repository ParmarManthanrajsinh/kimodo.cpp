#pragma once

#include <string>
#include <vector>
#include "character/SkinningData.h"
#include "raylib.h"
#include "raymath.h"

namespace studio
{

struct CharacterBone
{
    std::string name;
    int parent = -1;
    Matrix local_transform{MatrixIdentity()};
    Matrix world_transform{MatrixIdentity()};
    Vector3 rest_position{0, 0, 0};
    Quaternion rest_rotation{0, 0, 0, 1};
    Vector3 rest_scale{1, 1, 1};
};

struct CharacterSubmesh
{
    int materialIndex = 0;
    uint32_t vertex_offset = 0;
    uint32_t vertex_count = 0;
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    Texture2D diffuse_texture{};
    Color base_color{255, 255, 255, 255};
    bool has_texture = false;
};

struct CharacterValidationReport
{
    bool valid = true;
    bool has_mesh = false;
    bool has_skeleton = false;
    bool has_skin = false;
    bool required_bones_present = false;
    bool valid_weights = false;
    bool valid_rest_pose = false;
    int vertex_count = 0;
    int triangle_count = 0;
    int bone_count = 0;
    float height = 0.0f;
    std::vector<std::string> detected_bones;
    std::vector<std::string> missing_required_bones;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class CharacterAsset
{
public:
    CharacterAsset() = default;
    ~CharacterAsset();

    // Move only
    CharacterAsset(const CharacterAsset&) = delete;
    CharacterAsset& operator=(const CharacterAsset&) = delete;
    CharacterAsset(CharacterAsset&& other) noexcept;
    CharacterAsset& operator=(CharacterAsset&& other) noexcept;

    void Unload();
    bool IsLoaded() const
    {
        return loaded;
    }

    const std::string& GetId() const
    {
        return Id;
    }
    const std::string& GetName() const
    {
        return Name;
    }
    const std::string& GetFilePath() const
    {
        return file_path;
    }
    const std::string& GetLicense() const
    {
        return license;
    }
    const std::string& GetAuthor() const
    {
        return author;
    }
    float GetScale() const
    {
        return scale;
    }
    BoundingBox GetBounds() const
    {
        return bounds;
    }

    void SetId(std::string id)
    {
        Id = std::move(id);
    }
    void SetName(std::string name)
    {
        Name = std::move(name);
    }
    void SetFilePath(std::string path)
    {
        file_path = std::move(path);
    }
    void SetLicense(std::string lic)
    {
        license = std::move(lic);
    }
    void SetAuthor(std::string auth)
    {
        author = std::move(auth);
    }
    void SetScale(float s)
    {
        scale = s;
    }

    const std::vector<CharacterBone>& GetBones() const
    {
        return bones;
    }
    std::vector<CharacterBone>& GetBones()
    {
        return bones;
    }

    const std::vector<CharacterSubmesh>& GetSubmeshes() const
    {
        return submeshes;
    }
    std::vector<CharacterSubmesh>& GetSubmeshes()
    {
        return submeshes;
    }

    const SkinningData& GetSkinningData() const
    {
        return skinning_data;
    }
    SkinningData& GetSkinningData()
    {
        return skinning_data;
    }

    const CharacterValidationReport& GetValidationReport() const
    {
        return report;
    }
    void SetValidationReport(CharacterValidationReport in_report)
    {
        report = std::move(in_report);
    }

    int FindBoneIndex(const std::string& bone_name) const;

    // CPU animated vertices buffer (for CPU fallback skinning / wireframe)
    const std::vector<Vector3>& GetAnimatedVertices() const
    {
        return anim_vertices;
    }
    const std::vector<Vector3>& GetAnimatedNormals() const
    {
        return anim_normals;
    }
    void UpdateCpuSkinning(const std::vector<Matrix>& skin_matrices);

    void FinalizeGeometry(BoundingBox bounds);

private:
    bool loaded = false;
    std::string Id;
    std::string Name;
    std::string file_path;
    std::string license;
    std::string author;
    float scale = 1.0f;
    BoundingBox bounds{{0, 0, 0}, {0, 0, 0}};

    std::vector<CharacterBone> bones;
    std::vector<CharacterSubmesh> submeshes;
    SkinningData skinning_data;
    CharacterValidationReport report;

    // CPU skinning cached buffers
    std::vector<Vector3> anim_vertices;
    std::vector<Vector3> anim_normals;
};

} // namespace studio
