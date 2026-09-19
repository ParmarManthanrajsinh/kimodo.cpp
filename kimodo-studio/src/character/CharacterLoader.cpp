#include "character/CharacterLoader.h"
#include "character/SkinningData.h"
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <vector>

// Use cgltf declarations from raylib external header
#include "external/cgltf.h"

namespace studio
{
namespace
{

Matrix CgltfMatToRaylib(const cgltf_float m[16])
{
    Matrix r;
    r.m0 = m[0];
    r.m4 = m[4];
    r.m8 = m[8];
    r.m12 = m[12];
    r.m1 = m[1];
    r.m5 = m[5];
    r.m9 = m[9];
    r.m13 = m[13];
    r.m2 = m[2];
    r.m6 = m[6];
    r.m10 = m[10];
    r.m14 = m[14];
    r.m3 = m[3];
    r.m7 = m[7];
    r.m11 = m[11];
    r.m15 = m[15];
    return r;
}

int find_node_index(const cgltf_data* data, const cgltf_node* target)
{
    if (!data || !target)
        return -1;
    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        if (&data->nodes[i] == target)
            return static_cast<int>(i);
    }
    return -1;
}

int find_joint_in_skin(const cgltf_skin* skin, const cgltf_node* node)
{
    if (!skin || !node)
        return -1;
    for (size_t i = 0; i < skin->joints_count; ++i)
    {
        if (skin->joints[i] == node)
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace

bool CharacterLoader::LoadGLB(const std::string& file_path, CharacterAsset& out_asset, std::string& error)
{
    out_asset.Unload();

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result res = cgltf_parse_file(&options, file_path.c_str(), &data);
    if (res != cgltf_result_success || !data)
    {
        error = "Failed to parse glTF/GLB file: " + file_path;
        return false;
    }

    res = cgltf_load_buffers(&options, data, file_path.c_str());
    if (res != cgltf_result_success)
    {
        cgltf_free(data);
        error = "Failed to load glTF buffers: " + file_path;
        return false;
    }

    std::filesystem::path f_path(file_path);
    std::string filename = f_path.stem().string();
    out_asset.SetId(filename);
    out_asset.SetName(filename);
    out_asset.SetFilePath(file_path);

    // Default metadata
    if (filename == "CesiumMan")
    {
        out_asset.SetLicense("CC-BY 4.0");
        out_asset.SetAuthor("Cesium (Khronos glTF Sample Assets)");
    }
    else
    {
        out_asset.SetLicense("Open / User Imported");
        out_asset.SetAuthor("External");
    }

    // Process Skin / Skeleton
    const cgltf_skin* skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
    std::vector<CharacterBone> bones;
    std::vector<Matrix> inv_bind_matrices;

    if (skin && skin->joints_count > 0)
    {
        const size_t num_joints = skin->joints_count;
        bones.resize(num_joints);
        inv_bind_matrices.resize(num_joints, MatrixIdentity());

        // Read inverse bind matrices if present
        if (skin->inverse_bind_matrices)
        {
            std::vector<float> ibm_floats(num_joints * 16);
            cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, ibm_floats.data(), num_joints * 16);
            for (size_t i = 0; i < num_joints; ++i)
            {
                inv_bind_matrices[i] = CgltfMatToRaylib(&ibm_floats[i * 16]);
            }
        }

        // Fill bones
        for (size_t i = 0; i < num_joints; ++i)
        {
            const cgltf_node* node = skin->joints[i];
            bones[i].name = node->name ? node->name : ("Joint_" + std::to_string(i));

            // Find parent within skin joints
            bones[i].parent = -1;
            if (node->parent)
            {
                bones[i].parent = find_joint_in_skin(skin, node->parent);
            }

            // Extract rest local transform
            cgltf_float loc_floats[16];
            cgltf_node_transform_local(node, loc_floats);
            bones[i].local_transform = CgltfMatToRaylib(loc_floats);

            if (!node->has_matrix)
            {
                Vector3 tr{node->has_translation ? node->translation[0] : 0.0f,
                           node->has_translation ? node->translation[1] : 0.0f,
                           node->has_translation ? node->translation[2] : 0.0f};
                Quaternion rot{
                    node->has_rotation ? node->rotation[0] : 0.0f, node->has_rotation ? node->rotation[1] : 0.0f,
                    node->has_rotation ? node->rotation[2] : 0.0f, node->has_rotation ? node->rotation[3] : 1.0f};
                Vector3 sc{node->has_scale ? node->scale[0] : 1.0f, node->has_scale ? node->scale[1] : 1.0f,
                           node->has_scale ? node->scale[2] : 1.0f};
                bones[i].rest_position = tr;
                bones[i].rest_rotation = rot;
                bones[i].rest_scale = sc;
            }

            // Extract full rest world transform (including scene and armature root ancestors)
            cgltf_float world_floats[16];
            cgltf_node_transform_world(node, world_floats);
            bones[i].world_transform = CgltfMatToRaylib(world_floats);
            inv_bind_matrices[i] = MatrixInvert(bones[i].world_transform);
        }
    }
    else
    {
        // Fallback: extract node hierarchy as pseudo-bones if no skin
        const size_t num_nodes = data->nodes_count;
        bones.resize(num_nodes);
        inv_bind_matrices.resize(num_nodes, MatrixIdentity());
        for (size_t i = 0; i < num_nodes; ++i)
        {
            const cgltf_node* node = &data->nodes[i];
            bones[i].name = node->name ? node->name : ("Node_" + std::to_string(i));
            bones[i].parent = find_node_index(data, node->parent);
            cgltf_float loc_floats[16];
            cgltf_node_transform_local(node, loc_floats);
            bones[i].local_transform = CgltfMatToRaylib(loc_floats);

            cgltf_float world_floats[16];
            cgltf_node_transform_world(node, world_floats);
            bones[i].world_transform = CgltfMatToRaylib(world_floats);
            inv_bind_matrices[i] = MatrixInvert(bones[i].world_transform);
        }
    }

    out_asset.GetBones() = std::move(bones);
    out_asset.GetSkinningData().inverse_bind_matrices = std::move(inv_bind_matrices);
    out_asset.GetSkinningData().current_bone_matrices.assign(out_asset.GetBones().size(), MatrixIdentity());
    out_asset.GetSkinningData().has_skin = (skin != nullptr && skin->joints_count > 0);

    // Process Meshes & Primitives
    SkinningData& s_data = out_asset.GetSkinningData();
    std::vector<CharacterSubmesh>& submeshes = out_asset.GetSubmeshes();

    Vector3 min_bounds{1e9f, 1e9f, 1e9f};
    Vector3 max_bounds{-1e9f, -1e9f, -1e9f};

    for (size_t ni = 0; ni < data->nodes_count; ++ni)
    {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh)
            continue;
        const cgltf_mesh* mesh = node->mesh;

        // Mesh node world transform
        cgltf_float mesh_world_floats[16];
        cgltf_node_transform_world(node, mesh_world_floats);
        Matrix mesh_world_transform = CgltfMatToRaylib(mesh_world_floats);

        for (size_t pi = 0; pi < mesh->primitives_count; ++pi)
        {
            const cgltf_primitive* prim = &mesh->primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles)
                continue;

            const uint32_t vertex_base = static_cast<uint32_t>(s_data.vertices.size());
            const uint32_t index_base = static_cast<uint32_t>(s_data.indices.size());

            // Find accessors
            const cgltf_accessor* pos_acc = nullptr;
            const cgltf_accessor* norm_acc = nullptr;
            const cgltf_accessor* tex_acc = nullptr;
            const cgltf_accessor* joints_acc = nullptr;
            const cgltf_accessor* weights_acc = nullptr;

            for (size_t ai = 0; ai < prim->attributes_count; ++ai)
            {
                const auto& attr = prim->attributes[ai];
                if (attr.type == cgltf_attribute_type_position)
                    pos_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal)
                    norm_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord)
                    tex_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_joints)
                    joints_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_weights)
                    weights_acc = attr.data;
            }

            if (!pos_acc)
                continue;
            const size_t vcount = pos_acc->count;

            std::vector<float> pos_floats(vcount * 3);
            cgltf_accessor_unpack_floats(pos_acc, pos_floats.data(), vcount * 3);

            std::vector<float> norm_floats(vcount * 3, 0.0f);
            if (norm_acc)
            {
                cgltf_accessor_unpack_floats(norm_acc, norm_floats.data(), vcount * 3);
            }

            std::vector<float> tex_floats(vcount * 2, 0.0f);
            if (tex_acc)
            {
                cgltf_accessor_unpack_floats(tex_acc, tex_floats.data(), vcount * 2);
            }

            std::vector<float> joint_floats(vcount * 4, 0.0f);
            if (joints_acc)
            {
                cgltf_accessor_unpack_floats(joints_acc, joint_floats.data(), vcount * 4);
            }

            std::vector<float> weight_floats(vcount * 4, 0.0f);
            if (weights_acc)
            {
                cgltf_accessor_unpack_floats(weights_acc, weight_floats.data(), vcount * 4);
            }

            for (size_t v = 0; v < vcount; ++v)
            {
                SkinVertex vert;
                Vector3 raw_pos = {pos_floats[v * 3 + 0], pos_floats[v * 3 + 1], pos_floats[v * 3 + 2]};
                Vector3 raw_norm = {norm_floats[v * 3 + 0], norm_floats[v * 3 + 1], norm_floats[v * 3 + 2]};

                // Transform vertex position and normal to world bind space
                vert.position = Vector3Transform(raw_pos, mesh_world_transform);

                Vector3 norm_rot{mesh_world_transform.m0 * raw_norm.x + mesh_world_transform.m4 * raw_norm.y +
                                     mesh_world_transform.m8 * raw_norm.z,
                                 mesh_world_transform.m1 * raw_norm.x + mesh_world_transform.m5 * raw_norm.y +
                                     mesh_world_transform.m9 * raw_norm.z,
                                 mesh_world_transform.m2 * raw_norm.x + mesh_world_transform.m6 * raw_norm.y +
                                     mesh_world_transform.m10 * raw_norm.z};
                vert.normal = Vector3Normalize(norm_rot);
                vert.texcoord = {tex_floats[v * 2 + 0], tex_floats[v * 2 + 1]};

                if (joints_acc)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        vert.bone_indices[k] = static_cast<uint16_t>(std::round(joint_floats[v * 4 + k]));
                    }
                }
                if (weights_acc)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        vert.bone_weights[k] = weight_floats[v * 4 + k];
                        sum += vert.bone_weights[k];
                    }
                    if (sum > 1e-4f)
                    {
                        for (int k = 0; k < 4; ++k)
                            vert.bone_weights[k] /= sum;
                    }
                }

                // Bounds update
                min_bounds.x = std::min(min_bounds.x, vert.position.x);
                min_bounds.y = std::min(min_bounds.y, vert.position.y);
                min_bounds.z = std::min(min_bounds.z, vert.position.z);
                max_bounds.x = std::max(max_bounds.x, vert.position.x);
                max_bounds.y = std::max(max_bounds.y, vert.position.y);
                max_bounds.z = std::max(max_bounds.z, vert.position.z);

                s_data.vertices.push_back(vert);
            }

            // Unpack indices
            uint32_t icount = 0;
            if (prim->indices)
            {
                icount = static_cast<uint32_t>(prim->indices->count);
                for (size_t idx = 0; idx < prim->indices->count; ++idx)
                {
                    cgltf_size i_val = cgltf_accessor_read_index(prim->indices, idx);
                    s_data.indices.push_back(vertex_base + static_cast<uint32_t>(i_val));
                }
            }
            else
            {
                icount = static_cast<uint32_t>(vcount);
                for (uint32_t idx = 0; idx < icount; ++idx)
                {
                    s_data.indices.push_back(vertex_base + idx);
                }
            }

            CharacterSubmesh sub;
            sub.vertex_offset = vertex_base;
            sub.vertex_count = static_cast<uint32_t>(vcount);
            sub.index_offset = index_base;
            sub.index_count = icount;

            // Load material diffuse texture / color if available
            if (prim->material)
            {
                const auto* mat = prim->material;
                if (mat->has_pbr_metallic_roughness)
                {
                    const auto& pbr = mat->pbr_metallic_roughness;
                    sub.base_color = Color{static_cast<unsigned char>(pbr.base_color_factor[0] * 255),
                                           static_cast<unsigned char>(pbr.base_color_factor[1] * 255),
                                           static_cast<unsigned char>(pbr.base_color_factor[2] * 255),
                                           static_cast<unsigned char>(pbr.base_color_factor[3] * 255)};
                    if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image)
                    {
                        const auto* img = pbr.base_color_texture.texture->image;
                        if (img->buffer_view && img->buffer_view->buffer && img->buffer_view->buffer->data)
                        {
                            const unsigned char* bytes =
                                static_cast<const unsigned char*>(img->buffer_view->buffer->data) +
                                img->buffer_view->offset;
                            int size = static_cast<int>(img->buffer_view->size);
                            const char* ext =
                                (img->mime_type && std::string(img->mime_type).find("jpeg") != std::string::npos)
                                    ? ".jpg"
                                    : ".png";
                            Image right_img = LoadImageFromMemory(ext, bytes, size);
                            if (right_img.data)
                            {
                                if (IsWindowReady())
                                {
                                    sub.diffuse_texture = LoadTextureFromImage(right_img);
                                    sub.has_texture = (sub.diffuse_texture.id > 0);
                                }
                                UnloadImage(right_img);
                            }
                        }
                    }
                }
            }

            submeshes.push_back(sub);
        }
    }

    cgltf_free(data);

    if (s_data.vertices.empty())
    {
        error = "No triangle meshes found in character file";
        return false;
    }

    BoundingBox bounds{min_bounds, max_bounds};
    out_asset.FinalizeGeometry(bounds);

    // Validate
    CharacterValidationReport rep = Validate(out_asset);
    out_asset.SetValidationReport(rep);

    return true;
}

CharacterValidationReport CharacterLoader::Validate(const CharacterAsset& asset)
{
    CharacterValidationReport rep;
    rep.has_mesh = !asset.GetSkinningData().vertices.empty();
    rep.vertex_count = static_cast<int>(asset.GetSkinningData().vertices.size());
    rep.triangle_count = static_cast<int>(asset.GetSkinningData().indices.size() / 3);
    rep.bone_count = static_cast<int>(asset.GetBones().size());
    rep.has_skeleton = (rep.bone_count > 0);
    rep.has_skin = asset.GetSkinningData().has_skin;

    const BoundingBox b = asset.GetBounds();
    rep.height = (b.max.y - b.min.y);

    for (const auto& bone : asset.GetBones())
    {
        rep.detected_bones.push_back(bone.name);
    }

    // Check required humanoid bones (essential subset)
    const std::vector<std::string> required_keywords = {"hips", "spine", "head", "arm", "hand", "leg", "foot"};

    std::set<std::string> matched_keywords;
    for (const auto& bone_name : rep.detected_bones)
    {
        std::string lower = bone_name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto& kw : required_keywords)
        {
            if (lower.find(kw) != std::string::npos || (kw == "hips" && lower.find("pelvis") != std::string::npos) ||
                (kw == "head" && lower.find("neck") != std::string::npos) ||
                (kw == "leg" && (lower.find("thigh") != std::string::npos || lower.find("calf") != std::string::npos ||
                                 lower.find("shin") != std::string::npos)))
            {
                matched_keywords.insert(kw);
            }
        }
    }

    for (const auto& kw : required_keywords)
    {
        if (matched_keywords.find(kw) == matched_keywords.end())
        {
            rep.missing_required_bones.push_back(kw);
        }
    }

    rep.required_bones_present = (matched_keywords.size() >= 4); // at least 4 major humanoid bone groups
    rep.valid_weights = true;
    for (const auto& v : asset.GetSkinningData().vertices)
    {
        float sum = v.bone_weights[0] + v.bone_weights[1] + v.bone_weights[2] + v.bone_weights[3];
        if (std::abs(sum - 1.0f) > 0.1f && sum > 1e-4f)
        {
            rep.valid_weights = false;
            rep.warnings.push_back("Unnormalized vertex weights detected");
            break;
        }
    }

    rep.valid_rest_pose = (rep.height > 0.5f && rep.height < 3.0f);
    if (!rep.valid_rest_pose)
    {
        rep.warnings.push_back("Character height unusual: " + std::to_string(rep.height) + "m");
    }

    rep.valid = rep.has_mesh && rep.has_skeleton;
    return rep;
}

} // namespace studio
