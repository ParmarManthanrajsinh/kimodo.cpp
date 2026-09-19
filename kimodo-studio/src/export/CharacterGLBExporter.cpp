#include "export/CharacterGLBExporter.h"
#include "export/ExportPreset.h"
#include "raymath.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace studio {
namespace {

struct BinBuilder {
    std::vector<uint8_t> data;

    size_t append(const void* src, size_t bytes) {
        // 4-byte align
        while (data.size() % 4 != 0)
            data.push_back(0);
        size_t offset = data.size();
        const uint8_t* p = static_cast<const uint8_t*>(src);
        data.insert(data.end(), p, p + bytes);
        return offset;
    }
};

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

} // namespace

bool CharacterGLBExporter::ExportCharacterGLB(const CharacterAsset& character, const Animation& animation,
                                              const CharacterBoneMap& mapping, const ExportOptions& options,
                                              std::string& error, std::string* report) {
    if (!character.IsLoaded() || character.GetSkinningData().vertices.empty()) {
        error = "Character is empty or not loaded";
        return false;
    }
    if (animation.empty()) {
        error = "Animation is empty";
        return false;
    }

    const size_t vcount = character.GetSkinningData().vertices.size();
    const size_t icount = character.GetSkinningData().indices.size();
    const size_t bcount = character.GetBones().size();
    const int F = animation.frames;
    const float fps = options.fps > 0 ? options.fps : animation.fps;

    BinBuilder bin;

    // 1. Pack Positions (vec3 float)
    std::vector<float> pos_data(vcount * 3);
    Vector3 min_pos{1e9f, 1e9f, 1e9f}, max_pos{-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < vcount; ++i) {
        const auto& p = character.GetSkinningData().vertices[i].position;
        pos_data[i * 3 + 0] = p.x;
        pos_data[i * 3 + 1] = p.y;
        pos_data[i * 3 + 2] = p.z;
        min_pos.x = std::min(min_pos.x, p.x);
        min_pos.y = std::min(min_pos.y, p.y);
        min_pos.z = std::min(min_pos.z, p.z);
        max_pos.x = std::max(max_pos.x, p.x);
        max_pos.y = std::max(max_pos.y, p.y);
        max_pos.z = std::max(max_pos.z, p.z);
    }
    size_t off_pos = bin.append(pos_data.data(), pos_data.size() * sizeof(float));

    // 2. Pack Normals (vec3 float)
    std::vector<float> norm_data(vcount * 3);
    for (size_t i = 0; i < vcount; ++i) {
        const auto& n = character.GetSkinningData().vertices[i].normal;
        norm_data[i * 3 + 0] = n.x;
        norm_data[i * 3 + 1] = n.y;
        norm_data[i * 3 + 2] = n.z;
    }
    size_t off_norm = bin.append(norm_data.data(), norm_data.size() * sizeof(float));

    // 3. Pack UVs (vec2 float)
    std::vector<float> uv_data(vcount * 2);
    for (size_t i = 0; i < vcount; ++i) {
        const auto& u = character.GetSkinningData().vertices[i].texcoord;
        uv_data[i * 2 + 0] = u.x;
        uv_data[i * 2 + 1] = u.y;
    }
    size_t off_uv = bin.append(uv_data.data(), uv_data.size() * sizeof(float));

    // 4. Pack Joints (vec4 unsigned short)
    std::vector<uint16_t> joints_data(vcount * 4);
    for (size_t i = 0; i < vcount; ++i) {
        for (int k = 0; k < 4; ++k) {
            joints_data[i * 4 + k] = character.GetSkinningData().vertices[i].bone_indices[k];
        }
    }
    size_t off_joints = bin.append(joints_data.data(), joints_data.size() * sizeof(uint16_t));

    // 5. Pack Weights (vec4 float)
    std::vector<float> weights_data(vcount * 4);
    for (size_t i = 0; i < vcount; ++i) {
        for (int k = 0; k < 4; ++k) {
            weights_data[i * 4 + k] = character.GetSkinningData().vertices[i].bone_weights[k];
        }
    }
    size_t off_weights = bin.append(weights_data.data(), weights_data.size() * sizeof(float));

    // 6. Pack Indices (scalar unsigned int)
    std::vector<uint32_t> idx_data(icount);
    for (size_t i = 0; i < icount; ++i)
        idx_data[i] = character.GetSkinningData().indices[i];
    size_t off_indices = bin.append(idx_data.data(), idx_data.size() * sizeof(uint32_t));

    // 7. Pack Inverse Bind Matrices (mat4 float)
    std::vector<float> ibm_data(bcount * 16);
    for (size_t b = 0; b < bcount; ++b) {
        const Matrix& m = (b < character.GetSkinningData().inverse_bind_matrices.size())
                              ? character.GetSkinningData().inverse_bind_matrices[b]
                              : MatrixIdentity();
        float* dst = &ibm_data[b * 16];
        dst[0] = m.m0;
        dst[1] = m.m1;
        dst[2] = m.m2;
        dst[3] = m.m3;
        dst[4] = m.m4;
        dst[5] = m.m5;
        dst[6] = m.m6;
        dst[7] = m.m7;
        dst[8] = m.m8;
        dst[9] = m.m9;
        dst[10] = m.m10;
        dst[11] = m.m11;
        dst[12] = m.m12;
        dst[13] = m.m13;
        dst[14] = m.m14;
        dst[15] = m.m15;
    }
    size_t off_ibm = bin.append(ibm_data.data(), ibm_data.size() * sizeof(float));

    // 8. Pack Animation Timestamps (scalar float)
    std::vector<float> times(F);
    for (int f = 0; f < F; ++f)
        times[f] = (fps > 0.0f) ? (static_cast<float>(f) / fps) : 0.0f;
    size_t off_time = bin.append(times.data(), times.size() * sizeof(float));

    // 9. Pack Animation Rotations per bone
    std::map<std::string, int> anim_joint_map;
    for (int j = 0; j < animation.joints; ++j) {
        anim_joint_map[animation.joint_names[j]] = j;
    }

    std::vector<size_t> off_rots(bcount);
    for (size_t b = 0; b < bcount; ++b) {
        std::vector<float> rot_data(F * 4);
        const auto& bone = character.GetBones()[b];
        auto map_it = mapping.find(bone.name);
        int src_j = -1;
        if (map_it != mapping.end() && !map_it->second.empty() && map_it->second != "(none)") {
            auto src_it = anim_joint_map.find(map_it->second);
            if (src_it != anim_joint_map.end())
                src_j = src_it->second;
        }

        for (int f = 0; f < F; ++f) {
            if (src_j >= 0) {
                const float* q =
                    animation.local_rotations_xyzw.data() + (static_cast<size_t>(f) * animation.joints + src_j) * 4;
                rot_data[f * 4 + 0] = q[0];
                rot_data[f * 4 + 1] = q[1];
                rot_data[f * 4 + 2] = q[2];
                rot_data[f * 4 + 3] = q[3];
            } else {
                rot_data[f * 4 + 0] = bone.rest_rotation.x;
                rot_data[f * 4 + 1] = bone.rest_rotation.y;
                rot_data[f * 4 + 2] = bone.rest_rotation.z;
                rot_data[f * 4 + 3] = bone.rest_rotation.w;
            }
        }
        off_rots[b] = bin.append(rot_data.data(), rot_data.size() * sizeof(float));
    }

    // 10. Pack Root Translation for Root/Pelvis joint
    std::vector<float> root_pos_data(F * 3);
    for (int f = 0; f < F; ++f) {
        root_pos_data[f * 3 + 0] = animation.root_positions[f * 3 + 0];
        root_pos_data[f * 3 + 1] = animation.root_positions[f * 3 + 1];
        root_pos_data[f * 3 + 2] = animation.root_positions[f * 3 + 2];
    }
    size_t off_root_trans = bin.append(root_pos_data.data(), root_pos_data.size() * sizeof(float));

    // Pad binary chunk to 4 bytes
    while (bin.data.size() % 4 != 0)
        bin.data.push_back(0);

    // Build JSON Structure
    std::ostringstream ss;
    ss << "{\n"
       << "  \"asset\": {\"version\": \"2.0\", \"generator\": \"Kimodo Studio Character Exporter\"},\n"
       << "  \"scene\": 0,\n"
       << "  \"scenes\": [{\"nodes\": [0, 1]}],\n" // node 0 = Mesh Node, node 1 = Skeleton Root
       << "  \"nodes\": [\n"
       << "    {\"name\": \"CharacterMesh\", \"mesh\": 0, \"skin\": 0},\n"; // Node 0

    // Joint Nodes (Node 1 to 1 + bcount)
    for (size_t b = 0; b < bcount; ++b) {
        const auto& bone = character.GetBones()[b];
        ss << "    {\"name\": \"" << json_escape(bone.name) << "\"";

        // Collect children
        std::vector<int> child_nodes;
        for (size_t c = 0; c < bcount; ++c) {
            if (character.GetBones()[c].parent == static_cast<int>(b)) {
                child_nodes.push_back(static_cast<int>(1 + c));
            }
        }
        if (!child_nodes.empty()) {
            ss << ", \"children\": [";
            for (size_t k = 0; k < child_nodes.size(); ++k) {
                ss << child_nodes[k] << (k + 1 < child_nodes.size() ? ", " : "");
            }
            ss << "]";
        }

        // Translation
        ss << ", \"translation\": [" << bone.rest_position.x << ", " << bone.rest_position.y << ", "
           << bone.rest_position.z << "]";
        ss << ", \"rotation\": [" << bone.rest_rotation.x << ", " << bone.rest_rotation.y << ", "
           << bone.rest_rotation.z << ", " << bone.rest_rotation.w << "]";
        ss << "}" << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Meshes
    ss << "  \"meshes\": [{\n"
       << "    \"name\": \"CharacterMesh\",\n"
       << "    \"primitives\": [{\n"
       << "      \"attributes\": {\n"
       << "        \"POSITION\": 0,\n"
       << "        \"NORMAL\": 1,\n"
       << "        \"TEXCOORD_0\": 2,\n"
       << "        \"JOINTS_0\": 3,\n"
       << "        \"WEIGHTS_0\": 4\n"
       << "      },\n"
       << "      \"indices\": 5\n"
       << "    }]\n"
       << "  }],\n";

    // Skins
    ss << "  \"skins\": [{\n"
       << "    \"inverse_bind_matrices\": 6,\n"
       << "    \"joints\": [";
    for (size_t b = 0; b < bcount; ++b) {
        ss << (1 + b) << (b + 1 < bcount ? ", " : "");
    }
    ss << "]\n  }],\n";

    // Animations
    ss << "  \"animations\": [{\n"
       << "    \"name\": \"KimodoMotion\",\n"
       << "    \"channels\": [\n";
    // Translation channel for root (node 1)
    ss << "      {\"sampler\": 0, \"target\": {\"node\": 1, \"path\": \"translation\"}},\n";
    // Rotation channels for all joints
    for (size_t b = 0; b < bcount; ++b) {
        ss << "      {\"sampler\": " << (1 + b) << ", \"target\": {\"node\": " << (1 + b)
           << ", \"path\": \"rotation\"}}" << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "    ],\n"
       << "    \"samplers\": [\n"
       << "      {\"input\": 7, \"interpolation\": \"LINEAR\", \"output\": 8},\n"; // Root Translation sampler (sampler
                                                                                   // 0)
    for (size_t b = 0; b < bcount; ++b) {
        ss << "      {\"input\": 7, \"interpolation\": \"LINEAR\", \"output\": " << (9 + b) << "}"
           << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "    ]\n  }],\n";

    // Buffers & BufferViews & Accessors
    // BufferViews
    ss << "  \"buffers\": [{\"byteLength\": " << bin.data.size() << "}],\n"
       << "  \"bufferViews\": [\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << off_pos << ", \"byteLength\": " << (vcount * 3 * sizeof(float))
       << ", \"target\": 34962},\n" // 0: pos
       << "    {\"buffer\": 0, \"byteOffset\": " << off_norm << ", \"byteLength\": " << (vcount * 3 * sizeof(float))
       << ", \"target\": 34962},\n" // 1: norm
       << "    {\"buffer\": 0, \"byteOffset\": " << off_uv << ", \"byteLength\": " << (vcount * 2 * sizeof(float))
       << ", \"target\": 34962},\n" // 2: uv
       << "    {\"buffer\": 0, \"byteOffset\": " << off_joints
       << ", \"byteLength\": " << (vcount * 4 * sizeof(uint16_t)) << ", \"target\": 34962},\n" // 3: joints
       << "    {\"buffer\": 0, \"byteOffset\": " << off_weights << ", \"byteLength\": " << (vcount * 4 * sizeof(float))
       << ", \"target\": 34962},\n" // 4: weights
       << "    {\"buffer\": 0, \"byteOffset\": " << off_indices << ", \"byteLength\": " << (icount * sizeof(uint32_t))
       << ", \"target\": 34963},\n" // 5: indices
       << "    {\"buffer\": 0, \"byteOffset\": " << off_ibm << ", \"byteLength\": " << (bcount * 16 * sizeof(float))
       << "},\n" // 6: ibm
       << "    {\"buffer\": 0, \"byteOffset\": " << off_time << ", \"byteLength\": " << (F * sizeof(float))
       << "},\n" // 7: time
       << "    {\"buffer\": 0, \"byteOffset\": " << off_root_trans << ", \"byteLength\": " << (F * 3 * sizeof(float))
       << "},\n"; // 8: root trans
    for (size_t b = 0; b < bcount; ++b) {
        ss << "    {\"buffer\": 0, \"byteOffset\": " << off_rots[b] << ", \"byteLength\": " << (F * 4 * sizeof(float))
           << "}" << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Accessors
    ss << "  \"accessors\": [\n"
       << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": " << vcount << ", \"type\": \"VEC3\", \"min\": ["
       << min_pos.x << ", " << min_pos.y << ", " << min_pos.z << "], \"max\": [" << max_pos.x << ", " << max_pos.y
       << ", " << max_pos.z << "]},\n" // 0: pos
       << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": " << vcount
       << ", \"type\": \"VEC3\"},\n" // 1: norm
       << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": " << vcount
       << ", \"type\": \"VEC2\"},\n" // 2: uv
       << "    {\"bufferView\": 3, \"componentType\": 5123, \"count\": " << vcount
       << ", \"type\": \"VEC4\"},\n" // 3: joints
       << "    {\"bufferView\": 4, \"componentType\": 5126, \"count\": " << vcount
       << ", \"type\": \"VEC4\"},\n" // 4: weights
       << "    {\"bufferView\": 5, \"componentType\": 5125, \"count\": " << icount
       << ", \"type\": \"SCALAR\"},\n" // 5: indices
       << "    {\"bufferView\": 6, \"componentType\": 5126, \"count\": " << bcount
       << ", \"type\": \"MAT4\"},\n" // 6: ibm
       << "    {\"bufferView\": 7, \"componentType\": 5126, \"count\": " << F
       << ", \"type\": \"SCALAR\", \"min\": [0.0], \"max\": [" << (times.empty() ? 0.0f : times.back())
       << "]},\n" // 7: time
       << "    {\"bufferView\": 8, \"componentType\": 5126, \"count\": " << F
       << ", \"type\": \"VEC3\"},\n"; // 8: root trans
    for (size_t b = 0; b < bcount; ++b) {
        ss << "    {\"bufferView\": " << (9 + b) << ", \"componentType\": 5126, \"count\": " << F
           << ", \"type\": \"VEC4\"}" << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "  ]\n}\n";

    std::string json_str = ss.str();
    while (json_str.size() % 4 != 0)
        json_str += ' ';

    // Binary GLB layout:
    // Header (12B): magic 0x46546C67 ("glTF"), version 2, totalLength
    // Chunk 0 (JSON): chunkLength, chunkType 0x4E4F534A ("JSON"), json data
    // Chunk 1 (BIN): chunkLength, chunkType 0x004E4942 ("BIN\0"), bin data
    const uint32_t json_chunk_len = static_cast<uint32_t>(json_str.size());
    const uint32_t bin_chunk_len = static_cast<uint32_t>(bin.data.size());
    const uint32_t total_len = 12 + 8 + json_chunk_len + 8 + bin_chunk_len;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(options.path).parent_path(), ec);
    std::ofstream out(options.path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Cannot open output file: " + options.path;
        return false;
    }

    // Header
    const uint32_t magic = 0x46546C67;
    const uint32_t version = 2;
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&total_len), 4);

    // JSON Chunk
    const uint32_t json_type = 0x4E4F534A;
    out.write(reinterpret_cast<const char*>(&json_chunk_len), 4);
    out.write(reinterpret_cast<const char*>(&json_type), 4);
    out.write(json_str.data(), json_chunk_len);

    // BIN Chunk
    const uint32_t bin_type = 0x004E4942;
    out.write(reinterpret_cast<const char*>(&bin_chunk_len), 4);
    out.write(reinterpret_cast<const char*>(&bin_type), 4);
    out.write(reinterpret_cast<const char*>(bin.data.data()), bin_chunk_len);

    if (report) {
        *report = "Full Character GLB exported successfully (" + std::to_string(total_len / 1024) + " KB, " +
                  std::to_string(vcount) + " vertices, " + std::to_string(F) + " frames)";
    }

    return true;
}

} // namespace studio
