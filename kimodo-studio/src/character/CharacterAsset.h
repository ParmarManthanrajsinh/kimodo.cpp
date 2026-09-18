#pragma once

#include "character/SkinningData.h"
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>

namespace studio {

struct FCharacterBone {
    std::string name;
    int parent = -1;
    Matrix localTransform{MatrixIdentity()};
    Matrix worldTransform{MatrixIdentity()};
    Vector3 restPosition{0, 0, 0};
    Quaternion restRotation{0, 0, 0, 1};
    Vector3 restScale{1, 1, 1};
};

struct FCharacterSubmesh {
    int materialIndex = 0;
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    Texture2D diffuseTexture{};
    Color baseColor{255, 255, 255, 255};
    bool hasTexture = false;
};

struct FCharacterValidationReport {
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

class FCharacterAsset {
public:
    FCharacterAsset() = default;
    ~FCharacterAsset();

    // Move only
    FCharacterAsset(const FCharacterAsset&) = delete;
    FCharacterAsset& operator=(const FCharacterAsset&) = delete;
    FCharacterAsset(FCharacterAsset&& other) noexcept;
    FCharacterAsset& operator=(FCharacterAsset&& other) noexcept;

    void unload();
    bool IsLoaded() const { return bLoaded; }

    const std::string& GetId() const { return Id; }
    const std::string& GetName() const { return Name; }
    const std::string& GetFilePath() const { return filePath; }
    const std::string& GetLicense() const { return license; }
    const std::string& GetAuthor() const { return Author; }
    float GetScale() const { return Scale; }
    BoundingBox GetBounds() const { return Bounds; }

    void SetId(std::string id) { Id = std::move(id); }
    void SetName(std::string name) { Name = std::move(name); }
    void SetFilePath(std::string path) { filePath = std::move(path); }
    void SetLicense(std::string lic) { license = std::move(lic); }
    void SetAuthor(std::string auth) { Author = std::move(auth); }
    void SetScale(float s) { Scale = s; }

    const std::vector<FCharacterBone>& GetBones() const { return Bones; }
    std::vector<FCharacterBone>& GetBones() { return Bones; }

    const std::vector<FCharacterSubmesh>& GetSubmeshes() const { return submeshes; }
    std::vector<FCharacterSubmesh>& GetSubmeshes() { return submeshes; }

    const FSkinningData& GetSkinningData() const { return skinningData; }
    FSkinningData& GetSkinningData() { return skinningData; }

    const FCharacterValidationReport& GetValidationReport() const { return report; }
    void SetValidationReport(FCharacterValidationReport inReport) { report = std::move(inReport); }

    int findBoneIndex(const std::string& boneName) const;

    // CPU animated vertices buffer (for CPU fallback skinning / wireframe)
    const std::vector<Vector3>& GetAnimatedVertices() const { return AnimVertices; }
    const std::vector<Vector3>& GetAnimatedNormals() const { return AnimNormals; }
    void updateCpuSkinning(const std::vector<Matrix>& skinMatrices);

    void finalizeGeometry(BoundingBox bounds);

private:
    bool bLoaded = false;
    std::string Id;
    std::string Name;
    std::string filePath;
    std::string license;
    std::string Author;
    float Scale = 1.0f;
    BoundingBox Bounds{{0, 0, 0}, {0, 0, 0}};

    std::vector<FCharacterBone> Bones;
    std::vector<FCharacterSubmesh> submeshes;
    FSkinningData skinningData;
    FCharacterValidationReport report;

    // CPU skinning cached buffers
    std::vector<Vector3> AnimVertices;
    std::vector<Vector3> AnimNormals;
};

} // namespace studio
