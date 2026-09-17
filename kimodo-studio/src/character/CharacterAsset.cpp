#include "character/CharacterAsset.h"
#include "raymath.h"

#include <cmath>

namespace studio {

CharacterAsset::~CharacterAsset() {
    unload();
}

CharacterAsset::CharacterAsset(CharacterAsset&& other) noexcept
    : loaded_(other.loaded_),
      id_(std::move(other.id_)),
      name_(std::move(other.name_)),
      filePath_(std::move(other.filePath_)),
      license_(std::move(other.license_)),
      author_(std::move(other.author_)),
      scale_(other.scale_),
      bounds_(other.bounds_),
      bones_(std::move(other.bones_)),
      submeshes_(std::move(other.submeshes_)),
      skinningData_(std::move(other.skinningData_)),
      report_(std::move(other.report_)),
      animVertices_(std::move(other.animVertices_)),
      animNormals_(std::move(other.animNormals_)) {
    other.loaded_ = false;
}

CharacterAsset& CharacterAsset::operator=(CharacterAsset&& other) noexcept {
    if (this != &other) {
        unload();
        loaded_ = other.loaded_;
        id_ = std::move(other.id_);
        name_ = std::move(other.name_);
        filePath_ = std::move(other.filePath_);
        license_ = std::move(other.license_);
        author_ = std::move(other.author_);
        scale_ = other.scale_;
        bounds_ = other.bounds_;
        bones_ = std::move(other.bones_);
        submeshes_ = std::move(other.submeshes_);
        skinningData_ = std::move(other.skinningData_);
        report_ = std::move(other.report_);
        animVertices_ = std::move(other.animVertices_);
        animNormals_ = std::move(other.animNormals_);
        other.loaded_ = false;
    }
    return *this;
}

void CharacterAsset::unload() {
    for (auto& sub : submeshes_) {
        if (sub.hasTexture && sub.diffuseTexture.id > 0) {
            if (IsWindowReady()) {
                UnloadTexture(sub.diffuseTexture);
            }
            sub.diffuseTexture.id = 0;
            sub.hasTexture = false;
        }
    }
    submeshes_.clear();
    bones_.clear();
    skinningData_.vertices.clear();
    skinningData_.indices.clear();
    skinningData_.inverseBindMatrices.clear();
    skinningData_.currentBoneMatrices.clear();
    animVertices_.clear();
    animNormals_.clear();
    loaded_ = false;
}

int CharacterAsset::findBoneIndex(const std::string& boneName) const {
    for (size_t i = 0; i < bones_.size(); ++i) {
        if (bones_[i].name == boneName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CharacterAsset::finalizeGeometry(BoundingBox b) {
    bounds_ = b;
    const size_t vcount = skinningData_.vertices.size();
    animVertices_.resize(vcount);
    animNormals_.resize(vcount);
    for (size_t i = 0; i < vcount; ++i) {
        animVertices_[i] = skinningData_.vertices[i].position;
        animNormals_[i] = skinningData_.vertices[i].normal;
    }
    loaded_ = !skinningData_.vertices.empty();
}

void CharacterAsset::updateCpuSkinning(const std::vector<Matrix>& skinMatrices) {
    const size_t vcount = skinningData_.vertices.size();
    if (skinMatrices.empty() || animVertices_.size() != vcount) {
        return;
    }

    const int numBones = static_cast<int>(skinMatrices.size());

    for (size_t i = 0; i < vcount; ++i) {
        const SkinVertex& v = skinningData_.vertices[i];
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
            animVertices_[i] = posAccum;
            animNormals_[i] = Vector3Normalize(normAccum);
        } else {
            animVertices_[i] = v.position;
            animNormals_[i] = v.normal;
        }
    }
}

} // namespace studio
