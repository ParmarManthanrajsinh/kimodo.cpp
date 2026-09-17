#include "character/CharacterLoader.h"
#include "character/SkinningData.h"
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

// Use cgltf declarations from raylib external header
#include "external/cgltf.h"

namespace studio {
namespace {

Matrix cgltfMatToRaylib(const cgltf_float m[16]) {
    Matrix r;
    r.m0 = m[0];  r.m4 = m[4];  r.m8 = m[8];   r.m12 = m[12];
    r.m1 = m[1];  r.m5 = m[5];  r.m9 = m[9];   r.m13 = m[13];
    r.m2 = m[2];  r.m6 = m[6];  r.m10 = m[10]; r.m14 = m[14];
    r.m3 = m[3];  r.m7 = m[7];  r.m11 = m[11]; r.m15 = m[15];
    return r;
}

int findNodeIndex(const cgltf_data* data, const cgltf_node* target) {
    if (!data || !target) return -1;
    for (size_t i = 0; i < data->nodes_count; ++i) {
        if (&data->nodes[i] == target) return static_cast<int>(i);
    }
    return -1;
}

int findJointInSkin(const cgltf_skin* skin, const cgltf_node* node) {
    if (!skin || !node) return -1;
    for (size_t i = 0; i < skin->joints_count; ++i) {
        if (skin->joints[i] == node) return static_cast<int>(i);
    }
    return -1;
}

} // namespace

bool CharacterLoader::loadGLB(const std::string& filePath, CharacterAsset& outAsset,
                              std::string& error) {
    outAsset.unload();

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result res = cgltf_parse_file(&options, filePath.c_str(), &data);
    if (res != cgltf_result_success || !data) {
        error = "Failed to parse glTF/GLB file: " + filePath;
        return false;
    }

    res = cgltf_load_buffers(&options, data, filePath.c_str());
    if (res != cgltf_result_success) {
        cgltf_free(data);
        error = "Failed to load glTF buffers: " + filePath;
        return false;
    }

    std::filesystem::path fPath(filePath);
    std::string filename = fPath.stem().string();
    outAsset.setId(filename);
    outAsset.setName(filename);
    outAsset.setFilePath(filePath);

    // Default metadata
    if (filename == "CesiumMan") {
        outAsset.setLicense("CC-BY 4.0");
        outAsset.setAuthor("Cesium (Khronos glTF Sample Assets)");
    } else {
        outAsset.setLicense("Open / User Imported");
        outAsset.setAuthor("External");
    }

    // Process Skin / Skeleton
    const cgltf_skin* skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
    std::vector<CharacterBone> bones;
    std::vector<Matrix> invBindMatrices;

    if (skin && skin->joints_count > 0) {
        const size_t numJoints = skin->joints_count;
        bones.resize(numJoints);
        invBindMatrices.resize(numJoints, MatrixIdentity());

        // Read inverse bind matrices if present
        if (skin->inverse_bind_matrices) {
            std::vector<float> ibmFloats(numJoints * 16);
            cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, ibmFloats.data(), numJoints * 16);
            for (size_t i = 0; i < numJoints; ++i) {
                invBindMatrices[i] = cgltfMatToRaylib(&ibmFloats[i * 16]);
            }
        }

        // Fill bones
        for (size_t i = 0; i < numJoints; ++i) {
            const cgltf_node* node = skin->joints[i];
            bones[i].name = node->name ? node->name : ("Joint_" + std::to_string(i));

            // Find parent within skin joints
            bones[i].parent = -1;
            if (node->parent) {
                bones[i].parent = findJointInSkin(skin, node->parent);
            }

            // Extract rest local transform
            cgltf_float locFloats[16];
            cgltf_node_transform_local(node, locFloats);
            bones[i].localTransform = cgltfMatToRaylib(locFloats);

            if (!node->has_matrix) {
                Vector3 tr{node->has_translation ? node->translation[0] : 0.0f,
                           node->has_translation ? node->translation[1] : 0.0f,
                           node->has_translation ? node->translation[2] : 0.0f};
                Quaternion rot{node->has_rotation ? node->rotation[0] : 0.0f,
                               node->has_rotation ? node->rotation[1] : 0.0f,
                               node->has_rotation ? node->rotation[2] : 0.0f,
                               node->has_rotation ? node->rotation[3] : 1.0f};
                Vector3 sc{node->has_scale ? node->scale[0] : 1.0f,
                           node->has_scale ? node->scale[1] : 1.0f,
                           node->has_scale ? node->scale[2] : 1.0f};
                bones[i].restPosition = tr;
                bones[i].restRotation = rot;
                bones[i].restScale = sc;
            }

            // Extract full rest world transform (including scene and armature root ancestors)
            cgltf_float worldFloats[16];
            cgltf_node_transform_world(node, worldFloats);
            bones[i].worldTransform = cgltfMatToRaylib(worldFloats);
            invBindMatrices[i] = MatrixInvert(bones[i].worldTransform);
        }
    } else {
        // Fallback: extract node hierarchy as pseudo-bones if no skin
        const size_t numNodes = data->nodes_count;
        bones.resize(numNodes);
        invBindMatrices.resize(numNodes, MatrixIdentity());
        for (size_t i = 0; i < numNodes; ++i) {
            const cgltf_node* node = &data->nodes[i];
            bones[i].name = node->name ? node->name : ("Node_" + std::to_string(i));
            bones[i].parent = findNodeIndex(data, node->parent);
            cgltf_float locFloats[16];
            cgltf_node_transform_local(node, locFloats);
            bones[i].localTransform = cgltfMatToRaylib(locFloats);

            cgltf_float worldFloats[16];
            cgltf_node_transform_world(node, worldFloats);
            bones[i].worldTransform = cgltfMatToRaylib(worldFloats);
            invBindMatrices[i] = MatrixInvert(bones[i].worldTransform);
        }
    }

    outAsset.bones() = std::move(bones);
    outAsset.skinningData().inverseBindMatrices = std::move(invBindMatrices);
    outAsset.skinningData().currentBoneMatrices.assign(outAsset.bones().size(), MatrixIdentity());
    outAsset.skinningData().hasSkin = (skin != nullptr && skin->joints_count > 0);

    // Process Meshes & Primitives
    SkinningData& sData = outAsset.skinningData();
    std::vector<CharacterSubmesh>& submeshes = outAsset.submeshes();

    Vector3 minBounds{1e9f, 1e9f, 1e9f};
    Vector3 maxBounds{-1e9f, -1e9f, -1e9f};

    for (size_t ni = 0; ni < data->nodes_count; ++ni) {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh) continue;
        const cgltf_mesh* mesh = node->mesh;

        // Mesh node world transform
        cgltf_float meshWorldFloats[16];
        cgltf_node_transform_world(node, meshWorldFloats);
        Matrix meshWorldTransform = cgltfMatToRaylib(meshWorldFloats);

        for (size_t pi = 0; pi < mesh->primitives_count; ++pi) {
            const cgltf_primitive* prim = &mesh->primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            const uint32_t vertexBase = static_cast<uint32_t>(sData.vertices.size());
            const uint32_t indexBase = static_cast<uint32_t>(sData.indices.size());

            // Find accessors
            const cgltf_accessor* posAcc = nullptr;
            const cgltf_accessor* normAcc = nullptr;
            const cgltf_accessor* texAcc = nullptr;
            const cgltf_accessor* jointsAcc = nullptr;
            const cgltf_accessor* weightsAcc = nullptr;

            for (size_t ai = 0; ai < prim->attributes_count; ++ai) {
                const auto& attr = prim->attributes[ai];
                if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal) normAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord) texAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_joints) jointsAcc = attr.data;
                else if (attr.type == cgltf_attribute_type_weights) weightsAcc = attr.data;
            }

            if (!posAcc) continue;
            const size_t vcount = posAcc->count;

            std::vector<float> posFloats(vcount * 3);
            cgltf_accessor_unpack_floats(posAcc, posFloats.data(), vcount * 3);

            std::vector<float> normFloats(vcount * 3, 0.0f);
            if (normAcc) {
                cgltf_accessor_unpack_floats(normAcc, normFloats.data(), vcount * 3);
            }

            std::vector<float> texFloats(vcount * 2, 0.0f);
            if (texAcc) {
                cgltf_accessor_unpack_floats(texAcc, texFloats.data(), vcount * 2);
            }

            std::vector<float> jointFloats(vcount * 4, 0.0f);
            if (jointsAcc) {
                cgltf_accessor_unpack_floats(jointsAcc, jointFloats.data(), vcount * 4);
            }

            std::vector<float> weightFloats(vcount * 4, 0.0f);
            if (weightsAcc) {
                cgltf_accessor_unpack_floats(weightsAcc, weightFloats.data(), vcount * 4);
            }

            for (size_t v = 0; v < vcount; ++v) {
                SkinVertex vert;
                Vector3 rawPos = {posFloats[v * 3 + 0], posFloats[v * 3 + 1], posFloats[v * 3 + 2]};
                Vector3 rawNorm = {normFloats[v * 3 + 0], normFloats[v * 3 + 1], normFloats[v * 3 + 2]};

                // Transform vertex position and normal to world bind space
                vert.position = Vector3Transform(rawPos, meshWorldTransform);

                Vector3 normRot{
                    meshWorldTransform.m0 * rawNorm.x + meshWorldTransform.m4 * rawNorm.y + meshWorldTransform.m8 * rawNorm.z,
                    meshWorldTransform.m1 * rawNorm.x + meshWorldTransform.m5 * rawNorm.y + meshWorldTransform.m9 * rawNorm.z,
                    meshWorldTransform.m2 * rawNorm.x + meshWorldTransform.m6 * rawNorm.y + meshWorldTransform.m10 * rawNorm.z
                };
                vert.normal = Vector3Normalize(normRot);
                vert.texcoord = {texFloats[v * 2 + 0], texFloats[v * 2 + 1]};

                if (jointsAcc) {
                    for (int k = 0; k < 4; ++k) {
                        vert.boneIndices[k] = static_cast<uint16_t>(std::round(jointFloats[v * 4 + k]));
                    }
                }
                if (weightsAcc) {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k) {
                        vert.boneWeights[k] = weightFloats[v * 4 + k];
                        sum += vert.boneWeights[k];
                    }
                    if (sum > 1e-4f) {
                        for (int k = 0; k < 4; ++k) vert.boneWeights[k] /= sum;
                    }
                }

                // Bounds update
                minBounds.x = std::min(minBounds.x, vert.position.x);
                minBounds.y = std::min(minBounds.y, vert.position.y);
                minBounds.z = std::min(minBounds.z, vert.position.z);
                maxBounds.x = std::max(maxBounds.x, vert.position.x);
                maxBounds.y = std::max(maxBounds.y, vert.position.y);
                maxBounds.z = std::max(maxBounds.z, vert.position.z);

                sData.vertices.push_back(vert);
            }

            // Unpack indices
            uint32_t icount = 0;
            if (prim->indices) {
                icount = static_cast<uint32_t>(prim->indices->count);
                for (size_t idx = 0; idx < prim->indices->count; ++idx) {
                    cgltf_size iVal = cgltf_accessor_read_index(prim->indices, idx);
                    sData.indices.push_back(vertexBase + static_cast<uint32_t>(iVal));
                }
            } else {
                icount = static_cast<uint32_t>(vcount);
                for (uint32_t idx = 0; idx < icount; ++idx) {
                    sData.indices.push_back(vertexBase + idx);
                }
            }

            CharacterSubmesh sub;
            sub.vertexOffset = vertexBase;
            sub.vertexCount = static_cast<uint32_t>(vcount);
            sub.indexOffset = indexBase;
            sub.indexCount = icount;

            // Load material diffuse texture / color if available
            if (prim->material) {
                const auto* mat = prim->material;
                if (mat->has_pbr_metallic_roughness) {
                    const auto& pbr = mat->pbr_metallic_roughness;
                    sub.baseColor = Color{
                        static_cast<unsigned char>(pbr.base_color_factor[0] * 255),
                        static_cast<unsigned char>(pbr.base_color_factor[1] * 255),
                        static_cast<unsigned char>(pbr.base_color_factor[2] * 255),
                        static_cast<unsigned char>(pbr.base_color_factor[3] * 255)
                    };
                    if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                        const auto* img = pbr.base_color_texture.texture->image;
                        if (img->buffer_view && img->buffer_view->buffer && img->buffer_view->buffer->data) {
                            const unsigned char* bytes = static_cast<const unsigned char*>(img->buffer_view->buffer->data) + img->buffer_view->offset;
                            int size = static_cast<int>(img->buffer_view->size);
                            const char* ext = (img->mime_type && std::string(img->mime_type).find("jpeg") != std::string::npos) ? ".jpg" : ".png";
                            Image rImg = LoadImageFromMemory(ext, bytes, size);
                            if (rImg.data) {
                                if (IsWindowReady()) {
                                    sub.diffuseTexture = LoadTextureFromImage(rImg);
                                    sub.hasTexture = (sub.diffuseTexture.id > 0);
                                }
                                UnloadImage(rImg);
                            }
                        }
                    }
                }
            }

            submeshes.push_back(sub);
        }
    }

    cgltf_free(data);

    if (sData.vertices.empty()) {
        error = "No triangle meshes found in character file";
        return false;
    }

    BoundingBox bounds{minBounds, maxBounds};
    outAsset.finalizeGeometry(bounds);

    // Validate
    CharacterValidationReport rep = validate(outAsset);
    outAsset.setValidationReport(rep);

    return true;
}

CharacterValidationReport CharacterLoader::validate(const CharacterAsset& asset) {
    CharacterValidationReport rep;
    rep.hasMesh = !asset.skinningData().vertices.empty();
    rep.vertexCount = static_cast<int>(asset.skinningData().vertices.size());
    rep.triangleCount = static_cast<int>(asset.skinningData().indices.size() / 3);
    rep.boneCount = static_cast<int>(asset.bones().size());
    rep.hasSkeleton = (rep.boneCount > 0);
    rep.hasSkin = asset.skinningData().hasSkin;

    const BoundingBox b = asset.bounds();
    rep.height = (b.max.y - b.min.y);

    for (const auto& bone : asset.bones()) {
        rep.detectedBones.push_back(bone.name);
    }

    // Check required humanoid bones (essential subset)
    const std::vector<std::string> requiredKeywords = {
        "hips", "spine", "head", "arm", "hand", "leg", "foot"
    };

    std::set<std::string> matchedKeywords;
    for (const auto& bName : rep.detectedBones) {
        std::string lower = bName;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto& kw : requiredKeywords) {
            if (lower.find(kw) != std::string::npos ||
                (kw == "hips" && lower.find("pelvis") != std::string::npos) ||
                (kw == "head" && lower.find("neck") != std::string::npos) ||
                (kw == "leg" && (lower.find("thigh") != std::string::npos || lower.find("calf") != std::string::npos || lower.find("shin") != std::string::npos))) {
                matchedKeywords.insert(kw);
            }
        }
    }

    for (const auto& kw : requiredKeywords) {
        if (matchedKeywords.find(kw) == matchedKeywords.end()) {
            rep.missingRequiredBones.push_back(kw);
        }
    }

    rep.requiredBonesPresent = (matchedKeywords.size() >= 4); // at least 4 major humanoid bone groups
    rep.validWeights = true;
    for (const auto& v : asset.skinningData().vertices) {
        float sum = v.boneWeights[0] + v.boneWeights[1] + v.boneWeights[2] + v.boneWeights[3];
        if (std::abs(sum - 1.0f) > 0.1f && sum > 1e-4f) {
            rep.validWeights = false;
            rep.warnings.push_back("Unnormalized vertex weights detected");
            break;
        }
    }

    rep.validRestPose = (rep.height > 0.5f && rep.height < 3.0f);
    if (!rep.validRestPose) {
        rep.warnings.push_back("Character height unusual: " + std::to_string(rep.height) + "m");
    }

    rep.valid = rep.hasMesh && rep.hasSkeleton;
    return rep;
}

} // namespace studio
