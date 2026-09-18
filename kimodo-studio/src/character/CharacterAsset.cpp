#include "character/CharacterAsset.h"
#include "raymath.h"

#include <cmath>

namespace studio {

FCharacterAsset::~FCharacterAsset() {
    unload();
}

FCharacterAsset::FCharacterAsset(FCharacterAsset&& other) noexcept
    : bLoaded(other.bLoaded),
      Id(std::move(other.Id)),
      Name(std::move(other.Name)),
      filePath(std::move(other.filePath)),
      license(std::move(other.license)),
      Author(std::move(other.Author)),
      Scale(other.Scale),
      Bounds(other.Bounds),
      Bones(std::move(other.Bones)),
      submeshes(std::move(other.submeshes)),
      skinningData(std::move(other.skinningData)),
      report(std::move(other.report)),
      AnimVertices(std::move(other.AnimVertices)),
      AnimNormals(std::move(other.AnimNormals)) {
    other.bLoaded = false;
}

FCharacterAsset& FCharacterAsset::operator=(FCharacterAsset&& other) noexcept {
    if (this != &other) {
        unload();
        bLoaded = other.bLoaded;
        Id = std::move(other.Id);
        Name = std::move(other.Name);
        filePath = std::move(other.filePath);
        license = std::move(other.license);
        Author = std::move(other.Author);
        Scale = other.Scale;
        Bounds = other.Bounds;
        Bones = std::move(other.Bones);
        submeshes = std::move(other.submeshes);
        skinningData = std::move(other.skinningData);
        report = std::move(other.report);
        AnimVertices = std::move(other.AnimVertices);
        AnimNormals = std::move(other.AnimNormals);
        other.bLoaded = false;
    }
    return *this;
}

void FCharacterAsset::unload() {
    for (auto& sub : submeshes) {
        if (sub.hasTexture && sub.diffuseTexture.id > 0) {
            if (IsWindowReady()) {
                UnloadTexture(sub.diffuseTexture);
            }
            sub.diffuseTexture.id = 0;
            sub.hasTexture = false;
        }
    }
    submeshes.clear();
    Bones.clear();
    skinningData.vertices.clear();
    skinningData.indices.clear();
    skinningData.inverseBindMatrices.clear();
    skinningData.currentBoneMatrices.clear();
    AnimVertices.clear();
    AnimNormals.clear();
    bLoaded = false;
}

int FCharacterAsset::findBoneIndex(const std::string& boneName) const {
    for (size_t i = 0; i < Bones.size(); ++i) {
        if (Bones[i].name == boneName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void FCharacterAsset::finalizeGeometry(BoundingBox b) {
    Bounds = b;
    const size_t vcount = skinningData.vertices.size();
    AnimVertices.resize(vcount);
    AnimNormals.resize(vcount);
    for (size_t i = 0; i < vcount; ++i) {
        AnimVertices[i] = skinningData.vertices[i].position;
        AnimNormals[i] = skinningData.vertices[i].normal;
    }
    bLoaded = !skinningData.vertices.empty();
}

void FCharacterAsset::updateCpuSkinning(const std::vector<Matrix>& skinMatrices) {
    const size_t vcount = skinningData.vertices.size();
    if (skinMatrices.empty() || AnimVertices.size() != vcount) {
        return;
    }

    const int numBones = static_cast<int>(skinMatrices.size());

    for (size_t i = 0; i < vcount; ++i) {
        const FSkinVertex& v = skinningData.vertices[i];
        Vector3 posAccum{0, 0, 0};
        Vector3 normAccum{0, 0, 0};
        float totalWeight = 0.0f;

        for (int k = 0; k < kMaxInfluences; ++k) {
            const float w = v.boneWeights[k];
            if (w <= 1e-4f) continue;
            const uint16_t bIdx = v.boneIndices[k];
            if (bIdx >= numBones) continue;

            const Matrix& m = skinMatrices[bIdx];
            Vector3 p = Vector3Transform(v.position, m);
            posAccum = Vector3Add(posAccum, Vector3Scale(p, w));

            // Transform normal (rotational part)
            Vector3 normRot{
                m.m0 * v.normal.x + m.m4 * v.normal.y + m.m8 * v.normal.z,
                m.m1 * v.normal.x + m.m5 * v.normal.y + m.m9 * v.normal.z,
                m.m2 * v.normal.x + m.m6 * v.normal.y + m.m10 * v.normal.z
            };
            normAccum = Vector3Add(normAccum, Vector3Scale(normRot, w));
            totalWeight += w;
        }

        if (totalWeight > 1e-4f) {
            AnimVertices[i] = posAccum;
            AnimNormals[i] = Vector3Normalize(normAccum);
        } else {
            AnimVertices[i] = v.position;
            AnimNormals[i] = v.normal;
        }
    }
}

} // namespace studio
