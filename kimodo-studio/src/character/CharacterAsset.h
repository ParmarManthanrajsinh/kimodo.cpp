#pragma once

#include "character/SkinningData.h"
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>

namespace studio {

struct CharacterBone {
    std::string name;
    int parent = -1;
    Matrix localTransform{MatrixIdentity()};
    Matrix worldTransform{MatrixIdentity()};
    Vector3 restPosition{0, 0, 0};
    Quaternion restRotation{0, 0, 0, 1};
    Vector3 restScale{1, 1, 1};
};

struct CharacterSubmesh {
    int materialIndex = 0;
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    Texture2D diffuseTexture{};
    Color baseColor{255, 255, 255, 255};
    bool hasTexture = false;
};

struct CharacterValidationReport {
    bool valid = true;
    bool hasMesh = false;
    bool hasSkeleton = false;
    bool hasSkin = false;
    bool requiredBonesPresent = false;
    bool validWeights = false;
    bool validRestPose = false;
    int vertexCount = 0;
    int triangleCount = 0;
    int boneCount = 0;
    float height = 0.0f;
    std::vector<std::string> detectedBones;
    std::vector<std::string> missingRequiredBones;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class CharacterAsset {
public:
    CharacterAsset() = default;
    ~CharacterAsset();

    // Move only
    CharacterAsset(const CharacterAsset&) = delete;
    CharacterAsset& operator=(const CharacterAsset&) = delete;
    CharacterAsset(CharacterAsset&& other) noexcept;
    CharacterAsset& operator=(CharacterAsset&& other) noexcept;

    void unload();
    bool isLoaded() const { return loaded_; }

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& filePath() const { return filePath_; }
    const std::string& license() const { return license_; }
    const std::string& author() const { return author_; }
    float scale() const { return scale_; }
    BoundingBox bounds() const { return bounds_; }

    void setId(std::string id) { id_ = std::move(id); }
    void setName(std::string name) { name_ = std::move(name); }
    void setFilePath(std::string path) { filePath_ = std::move(path); }
    void setLicense(std::string lic) { license_ = std::move(lic); }
    void setAuthor(std::string auth) { author_ = std::move(auth); }
    void setScale(float s) { scale_ = s; }

    const std::vector<CharacterBone>& bones() const { return bones_; }
    std::vector<CharacterBone>& bones() { return bones_; }

    const std::vector<CharacterSubmesh>& submeshes() const { return submeshes_; }
    std::vector<CharacterSubmesh>& submeshes() { return submeshes_; }

    const SkinningData& skinningData() const { return skinningData_; }
    SkinningData& skinningData() { return skinningData_; }

    const CharacterValidationReport& validationReport() const { return report_; }
    void setValidationReport(CharacterValidationReport report) { report_ = std::move(report); }

    int findBoneIndex(const std::string& boneName) const;

    // CPU animated vertices buffer (for CPU fallback skinning / wireframe)
    const std::vector<Vector3>& animatedVertices() const { return animVertices_; }
    const std::vector<Vector3>& animatedNormals() const { return animNormals_; }
    void updateCpuSkinning(const std::vector<Matrix>& skinMatrices);

    void finalizeGeometry(BoundingBox bounds);

private:
    bool loaded_ = false;
    std::string id_;
    std::string name_;
    std::string filePath_;
    std::string license_;
    std::string author_;
    float scale_ = 1.0f;
    BoundingBox bounds_{{0, 0, 0}, {0, 0, 0}};

    std::vector<CharacterBone> bones_;
    std::vector<CharacterSubmesh> submeshes_;
    SkinningData skinningData_;
    CharacterValidationReport report_;

    // CPU skinning cached buffers
    std::vector<Vector3> animVertices_;
    std::vector<Vector3> animNormals_;
};

} // namespace studio
