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
        while (data.size() % 4 != 0) data.push_back(0);
        size_t offset = data.size();
        const uint8_t* p = static_cast<const uint8_t*>(src);
        data.insert(data.end(), p, p + bytes);
        return offset;
    }
};

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

} // namespace

bool CharacterGLBExporter::exportCharacterGLB(const CharacterAsset& character,
                                             const Animation& animation,
                                             const CharacterBoneMap& mapping,
                                             const ExportOptions& options,
                                             std::string& error,
                                             std::string* report) {
    if (!character.isLoaded() || character.skinningData().vertices.empty()) {
        error = "Character is empty or not loaded";
        return false;
    }
    if (animation.empty()) {
        error = "Animation is empty";
        return false;
    }

    const size_t vcount = character.skinningData().vertices.size();
    const size_t icount = character.skinningData().indices.size();
    const size_t bcount = character.bones().size();
    const int F = animation.frames;
    const float fps = options.fps > 0 ? options.fps : animation.fps;

    BinBuilder bin;

    // 1. Pack Positions (vec3 float)
    std::vector<float> posData(vcount * 3);
    Vector3 minPos{1e9f, 1e9f, 1e9f}, maxPos{-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < vcount; ++i) {
        const auto& p = character.skinningData().vertices[i].position;
        posData[i * 3 + 0] = p.x;
        posData[i * 3 + 1] = p.y;
        posData[i * 3 + 2] = p.z;
        minPos.x = std::min(minPos.x, p.x); minPos.y = std::min(minPos.y, p.y); minPos.z = std::min(minPos.z, p.z);
        maxPos.x = std::max(maxPos.x, p.x); maxPos.y = std::max(maxPos.y, p.y); maxPos.z = std::max(maxPos.z, p.z);
    }
    size_t offPos = bin.append(posData.data(), posData.size() * sizeof(float));

    // 2. Pack Normals (vec3 float)
    std::vector<float> normData(vcount * 3);
    for (size_t i = 0; i < vcount; ++i) {
        const auto& n = character.skinningData().vertices[i].normal;
        normData[i * 3 + 0] = n.x;
        normData[i * 3 + 1] = n.y;
        normData[i * 3 + 2] = n.z;
    }
    size_t offNorm = bin.append(normData.data(), normData.size() * sizeof(float));

    // 3. Pack UVs (vec2 float)
    std::vector<float> uvData(vcount * 2);
    for (size_t i = 0; i < vcount; ++i) {
        const auto& u = character.skinningData().vertices[i].texcoord;
        uvData[i * 2 + 0] = u.x;
        uvData[i * 2 + 1] = u.y;
    }
    size_t offUV = bin.append(uvData.data(), uvData.size() * sizeof(float));

    // 4. Pack Joints (vec4 unsigned short)
    std::vector<uint16_t> jointsData(vcount * 4);
    for (size_t i = 0; i < vcount; ++i) {
        for (int k = 0; k < 4; ++k) {
            jointsData[i * 4 + k] = character.skinningData().vertices[i].boneIndices[k];
        }
    }
    size_t offJoints = bin.append(jointsData.data(), jointsData.size() * sizeof(uint16_t));

    // 5. Pack Weights (vec4 float)
    std::vector<float> weightsData(vcount * 4);
    for (size_t i = 0; i < vcount; ++i) {
        for (int k = 0; k < 4; ++k) {
            weightsData[i * 4 + k] = character.skinningData().vertices[i].boneWeights[k];
        }
    }
    size_t offWeights = bin.append(weightsData.data(), weightsData.size() * sizeof(float));

    // 6. Pack Indices (scalar unsigned int)
    std::vector<uint32_t> idxData(icount);
    for (size_t i = 0; i < icount; ++i) idxData[i] = character.skinningData().indices[i];
    size_t offIndices = bin.append(idxData.data(), idxData.size() * sizeof(uint32_t));

    // 7. Pack Inverse Bind Matrices (mat4 float)
    std::vector<float> ibmData(bcount * 16);
    for (size_t b = 0; b < bcount; ++b) {
        const Matrix& m = (b < character.skinningData().inverseBindMatrices.size()) ?
                          character.skinningData().inverseBindMatrices[b] : MatrixIdentity();
        float* dst = &ibmData[b * 16];
        dst[0] = m.m0;  dst[1] = m.m1;  dst[2] = m.m2;  dst[3] = m.m3;
        dst[4] = m.m4;  dst[5] = m.m5;  dst[6] = m.m6;  dst[7] = m.m7;
        dst[8] = m.m8;  dst[9] = m.m9;  dst[10] = m.m10; dst[11] = m.m11;
        dst[12] = m.m12; dst[13] = m.m13; dst[14] = m.m14; dst[15] = m.m15;
    }
    size_t offIBM = bin.append(ibmData.data(), ibmData.size() * sizeof(float));

    // 8. Pack Animation Timestamps (scalar float)
    std::vector<float> times(F);
    for (int f = 0; f < F; ++f) times[f] = (fps > 0.0f) ? (static_cast<float>(f) / fps) : 0.0f;
    size_t offTime = bin.append(times.data(), times.size() * sizeof(float));

    // 9. Pack Animation Rotations per bone
    std::map<std::string, int> animJointMap;
    for (int j = 0; j < animation.joints; ++j) {
        animJointMap[animation.jointNames[j]] = j;
    }

    std::vector<size_t> offRots(bcount);
    for (size_t b = 0; b < bcount; ++b) {
        std::vector<float> rotData(F * 4);
        const auto& bone = character.bones()[b];
        auto mapIt = mapping.find(bone.name);
        int srcJ = -1;
        if (mapIt != mapping.end() && !mapIt->second.empty() && mapIt->second != "(none)") {
            auto srcIt = animJointMap.find(mapIt->second);
            if (srcIt != animJointMap.end()) srcJ = srcIt->second;
        }

        for (int f = 0; f < F; ++f) {
            if (srcJ >= 0) {
                const float* q = animation.localRotationsXyzw.data() + (static_cast<size_t>(f) * animation.joints + srcJ) * 4;
                rotData[f * 4 + 0] = q[0];
                rotData[f * 4 + 1] = q[1];
                rotData[f * 4 + 2] = q[2];
                rotData[f * 4 + 3] = q[3];
            } else {
                rotData[f * 4 + 0] = bone.restRotation.x;
                rotData[f * 4 + 1] = bone.restRotation.y;
                rotData[f * 4 + 2] = bone.restRotation.z;
                rotData[f * 4 + 3] = bone.restRotation.w;
            }
        }
        offRots[b] = bin.append(rotData.data(), rotData.size() * sizeof(float));
    }

    // 10. Pack Root Translation for Root/Pelvis joint
    std::vector<float> rootPosData(F * 3);
    for (int f = 0; f < F; ++f) {
        rootPosData[f * 3 + 0] = animation.rootPositions[f * 3 + 0];
        rootPosData[f * 3 + 1] = animation.rootPositions[f * 3 + 1];
        rootPosData[f * 3 + 2] = animation.rootPositions[f * 3 + 2];
    }
    size_t offRootTrans = bin.append(rootPosData.data(), rootPosData.size() * sizeof(float));

    // Pad binary chunk to 4 bytes
    while (bin.data.size() % 4 != 0) bin.data.push_back(0);

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
        const auto& bone = character.bones()[b];
        ss << "    {\"name\": \"" << jsonEscape(bone.name) << "\"";

        // Collect children
        std::vector<int> childNodes;
        for (size_t c = 0; c < bcount; ++c) {
            if (character.bones()[c].parent == static_cast<int>(b)) {
                childNodes.push_back(static_cast<int>(1 + c));
            }
        }
        if (!childNodes.empty()) {
            ss << ", \"children\": [";
            for (size_t k = 0; k < childNodes.size(); ++k) {
                ss << childNodes[k] << (k + 1 < childNodes.size() ? ", " : "");
            }
            ss << "]";
        }

        // Translation
        ss << ", \"translation\": [" << bone.restPosition.x << ", " << bone.restPosition.y << ", " << bone.restPosition.z << "]";
        ss << ", \"rotation\": [" << bone.restRotation.x << ", " << bone.restRotation.y << ", " << bone.restRotation.z << ", " << bone.restRotation.w << "]";
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
       << "    \"inverseBindMatrices\": 6,\n"
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
        ss << "      {\"sampler\": " << (1 + b) << ", \"target\": {\"node\": " << (1 + b) << ", \"path\": \"rotation\"}}"
           << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "    ],\n"
       << "    \"samplers\": [\n"
       << "      {\"input\": 7, \"interpolation\": \"LINEAR\", \"output\": 8},\n"; // Root Translation sampler (sampler 0)
    for (size_t b = 0; b < bcount; ++b) {
        ss << "      {\"input\": 7, \"interpolation\": \"LINEAR\", \"output\": " << (9 + b) << "}"
           << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "    ]\n  }],\n";

    // Buffers & BufferViews & Accessors
    // BufferViews
    ss << "  \"buffers\": [{\"byteLength\": " << bin.data.size() << "}],\n"
       << "  \"bufferViews\": [\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << offPos << ", \"byteLength\": " << (vcount * 3 * sizeof(float)) << ", \"target\": 34962},\n" // 0: pos
       << "    {\"buffer\": 0, \"byteOffset\": " << offNorm << ", \"byteLength\": " << (vcount * 3 * sizeof(float)) << ", \"target\": 34962},\n" // 1: norm
       << "    {\"buffer\": 0, \"byteOffset\": " << offUV << ", \"byteLength\": " << (vcount * 2 * sizeof(float)) << ", \"target\": 34962},\n" // 2: uv
       << "    {\"buffer\": 0, \"byteOffset\": " << offJoints << ", \"byteLength\": " << (vcount * 4 * sizeof(uint16_t)) << ", \"target\": 34962},\n" // 3: joints
       << "    {\"buffer\": 0, \"byteOffset\": " << offWeights << ", \"byteLength\": " << (vcount * 4 * sizeof(float)) << ", \"target\": 34962},\n" // 4: weights
       << "    {\"buffer\": 0, \"byteOffset\": " << offIndices << ", \"byteLength\": " << (icount * sizeof(uint32_t)) << ", \"target\": 34963},\n" // 5: indices
       << "    {\"buffer\": 0, \"byteOffset\": " << offIBM << ", \"byteLength\": " << (bcount * 16 * sizeof(float)) << "},\n" // 6: ibm
       << "    {\"buffer\": 0, \"byteOffset\": " << offTime << ", \"byteLength\": " << (F * sizeof(float)) << "},\n" // 7: time
       << "    {\"buffer\": 0, \"byteOffset\": " << offRootTrans << ", \"byteLength\": " << (F * 3 * sizeof(float)) << "},\n"; // 8: root trans
    for (size_t b = 0; b < bcount; ++b) {
        ss << "    {\"buffer\": 0, \"byteOffset\": " << offRots[b] << ", \"byteLength\": " << (F * 4 * sizeof(float)) << "}"
           << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Accessors
    ss << "  \"accessors\": [\n"
       << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": " << vcount << ", \"type\": \"VEC3\", \"min\": [" << minPos.x << ", " << minPos.y << ", " << minPos.z << "], \"max\": [" << maxPos.x << ", " << maxPos.y << ", " << maxPos.z << "]},\n" // 0: pos
       << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": " << vcount << ", \"type\": \"VEC3\"},\n" // 1: norm
       << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": " << vcount << ", \"type\": \"VEC2\"},\n" // 2: uv
       << "    {\"bufferView\": 3, \"componentType\": 5123, \"count\": " << vcount << ", \"type\": \"VEC4\"},\n" // 3: joints
       << "    {\"bufferView\": 4, \"componentType\": 5126, \"count\": " << vcount << ", \"type\": \"VEC4\"},\n" // 4: weights
       << "    {\"bufferView\": 5, \"componentType\": 5125, \"count\": " << icount << ", \"type\": \"SCALAR\"},\n" // 5: indices
       << "    {\"bufferView\": 6, \"componentType\": 5126, \"count\": " << bcount << ", \"type\": \"MAT4\"},\n" // 6: ibm
       << "    {\"bufferView\": 7, \"componentType\": 5126, \"count\": " << F << ", \"type\": \"SCALAR\", \"min\": [0.0], \"max\": [" << (times.empty() ? 0.0f : times.back()) << "]},\n" // 7: time
       << "    {\"bufferView\": 8, \"componentType\": 5126, \"count\": " << F << ", \"type\": \"VEC3\"},\n"; // 8: root trans
    for (size_t b = 0; b < bcount; ++b) {
        ss << "    {\"bufferView\": " << (9 + b) << ", \"componentType\": 5126, \"count\": " << F << ", \"type\": \"VEC4\"}"
           << (b + 1 < bcount ? ",\n" : "\n");
    }
    ss << "  ]\n}\n";

    std::string jsonStr = ss.str();
    while (jsonStr.size() % 4 != 0) jsonStr += ' ';

    // Binary GLB layout:
    // Header (12B): magic 0x46546C67 ("glTF"), version 2, totalLength
    // Chunk 0 (JSON): chunkLength, chunkType 0x4E4F534A ("JSON"), json data
    // Chunk 1 (BIN): chunkLength, chunkType 0x004E4942 ("BIN\0"), bin data
    const uint32_t jsonChunkLen = static_cast<uint32_t>(jsonStr.size());
    const uint32_t binChunkLen = static_cast<uint32_t>(bin.data.size());
    const uint32_t totalLen = 12 + 8 + jsonChunkLen + 8 + binChunkLen;

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
    out.write(reinterpret_cast<const char*>(&totalLen), 4);

    // JSON Chunk
    const uint32_t jsonType = 0x4E4F534A;
    out.write(reinterpret_cast<const char*>(&jsonChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&jsonType), 4);
    out.write(jsonStr.data(), jsonChunkLen);

    // BIN Chunk
    const uint32_t binType = 0x004E4942;
    out.write(reinterpret_cast<const char*>(&binChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&binType), 4);
    out.write(reinterpret_cast<const char*>(bin.data.data()), binChunkLen);

    if (report) {
        *report = "Full Character GLB exported successfully (" + std::to_string(totalLen / 1024) + " KB, " +
                  std::to_string(vcount) + " vertices, " + std::to_string(F) + " frames)";
    }

    return true;
}

} // namespace studio
